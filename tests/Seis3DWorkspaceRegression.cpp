#include "Seis3D_Sim.h"
#include "Seis3D/Internal3D.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

// Fail with a precise diagnostic rather than allowing a blank texture or invalid dock frame to pass.
void require3D(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}

// Read one rendered physical slice texel without relying on visual interpretation of a screenshot.
std::array<unsigned char,4> pixel3D(const Seis3D::Seis3D_State& s,int x,int y) {
    std::array<unsigned char,4> pixel{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER,s.gl3D.framebuffers3D[0]);
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    return pixel;
}

// Exercise real GPU extraction, GLSL compositing and dock frames using an invisible GL context.
int main(int argc,char**) {
    if(!glfwInit()) {std::puts("SKIP: GLFW unavailable");return 77;}
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window=glfwCreateWindow(1280,900,"Seis3D workspace regression",nullptr,nullptr);
    if(!window) {glfwTerminate();std::puts("SKIP: OpenGL 4.3 unavailable");return 77;}
    glfwMakeContextCurrent(window);glewExperimental=GL_TRUE;
    if(glewInit()!=GLEW_OK) {glfwDestroyWindow(window);glfwTerminate();return 77;}
    unsigned int count=0;int devices[8]{};
    if(cudaGLGetDevices(&count,devices,8,cudaGLDeviceListAll)!=cudaSuccess||!count) {
        glfwDestroyWindow(window);glfwTerminate();std::puts("SKIP: CUDA/GL interop unavailable");return 77;
    }
    cudaSetDevice(devices[0]);
    ImGui::CreateContext();ImPlot::CreateContext();
    ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    ImFontConfig fontConfig;fontConfig.SizePixels=21;ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 430");
    int result=0;
    try {
        Seis3D::Seis3D_State s;
        s.editContext3D.nx3D=33;s.editContext3D.ny3D=31;s.editContext3D.nz3D=29;s.editContext3D.pml3D=8;
        Seis3D::Detail3D::initializeSession3D(s,false);
        s.displayMode3D=2;
        Seis3D::Detail3D::updateGraphics3D(s);
        require3D(std::abs(s.materialRange3D[0]-3000)<1 && std::abs(s.materialRange3D[1]-4200)<1,"Material extrema / partial warp reduction failed");
        require3D(pixel3D(s,10,4)!=pixel3D(s,10,24),"Layered model has no visible material contrast");
        const auto materialPixel=pixel3D(s,10,4);
        s.displayMode3D=0;Seis3D::Detail3D::updateGraphics3D(s);
        require3D(pixel3D(s,10,4)==materialPixel,"Zero wavefield changed material background");
        s.displayMode3D=1;Seis3D::Detail3D::updateGraphics3D(s);
        require3D(pixel3D(s,10,4)==pixel3D(s,10,24),"Wave-only mode leaked model values");
        std::vector<float> velocity(s.context3D.cells3D,.25f);
        Seis3D::Detail3D::requireCuda3D(cudaMemcpy(s.gpu3D.field3D[2],velocity.data(),velocity.size()*sizeof(float),cudaMemcpyHostToDevice),"seed known display field");
        s.dirty3D=true;s.autoScale3D=false;s.manualScale3D=.25f;s.waveOpacity3D=1;s.waveThreshold3D=0;
        s.waveStyle3D=0;Seis3D::Detail3D::updateGraphics3D(s);const auto red=pixel3D(s,10,4);
        require3D(std::abs(s.viewMaximum3D-.25f)<1e-5f,"Wave/material channels were mixed");
        s.waveStyle3D=13;Seis3D::Detail3D::updateGraphics3D(s);const auto green=pixel3D(s,10,4);
        require3D(red!=green && red[0]>red[1] && green[1]>green[0],"Wave palette switch was not rendered");
        for(int property=5;property<=7;++property) {
            s.materialComponent3D=property;s.dirty3D=true;Seis3D::Detail3D::updateGraphics3D(s);
            require3D(std::isfinite(s.materialRange3D[0])&&s.materialRange3D[1]>s.materialRange3D[0],"Vp/Vs/density range failed");
        }
        // Exercise a real shot to populate the trace panel and clear the synthetic display field.
        s.materialComponent3D=5;s.displayMode3D=0;s.autoScale3D=true;s.waveOpacity3D=.9f;
        s.targetSamples3D=64;s.samplingStride3D=2;s.stepsPerFrame3D=8;
        Seis3D::Detail3D::startGather3D(s,1);
        for(int batch=0;batch<16;++batch) Seis3D::Detail3D::advanceSimulation3D(s);
        require3D(s.gather3D.sampleCount3D==64,"Display changes altered receiver acquisition cadence");
        for(int preset=0;preset<3;++preset) {
            s.layoutPreset3D=preset;s.resetLayout3D=true;
            for(int frame=0;frame<4;++frame) {
                ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
                Seis3D::renderScreen3D(s,1280,900);ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                require3D(!s.messageIsError3D,s.message3D.c_str());
            }
            if(argc>1) {
                std::vector<unsigned char> pixels(1280*900*3);
                glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
                glReadPixels(0,0,1280,900,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
                std::ofstream image("seis3d-layout-"+std::to_string(preset)+".ppm",std::ios::binary);
                image<<"P6\n1280 900\n255\n";
                for(int row=899;row>=0;--row) image.write(reinterpret_cast<const char*>(pixels.data()+row*1280*3),1280*3);
            }
            for(const char* name:{"Simulation controls###Seis3D.Controls","Inline X-Z###Seis3D.Inline","Spatial volume###Seis3D.Volume"}) {
                const auto* panel=ImGui::FindWindowByName(name);
                require3D(panel && panel->DockId!=0,"Panel did not join its isolated dockspace");
            }
        }
        require3D(glGetError()==GL_NO_ERROR,"OpenGL validation error");
        Seis3D::releaseSandbox3D(s);
        require3D(!s.ready3D&&!s.gl3D.interop3D&&!s.gpu3D.stream3D,"Cold release left resources alive");
        std::puts("PASS: material ranges, RG interop, real shader compositing, palettes, three dock presets, cold release");
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImPlot::DestroyContext();ImGui::DestroyContext();
    glfwDestroyWindow(window);glfwTerminate();return result;
}
