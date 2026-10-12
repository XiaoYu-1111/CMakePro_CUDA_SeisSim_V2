#include "Seis3D_Sim.h"
#include "Seis3D/Internal3D.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "AppTheme.h"
#include "HomeScreen.h"
#include "Cuda_Check.cuh"
#include "ResourcePaths.h"
#include <array>
#include <algorithm>
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

// Check the actual textured quad submitted to ImGui, rather than accepting a correct hidden FBO.
ImVec4 visibleSliceRect3D(GLuint texture) {
    for(const auto* list:ImGui::GetDrawData()->CmdLists) for(const auto& command:list->CmdBuffer) {
        if(command.GetTexID()!=ImTextureID(texture)||command.ElemCount<6) continue;
        ImVec4 rect(1e10f,1e10f,-1e10f,-1e10f);
        for(int i=0;i<6;++i) {
            const auto position=list->VtxBuffer[list->IdxBuffer[int(command.IdxOffset)+i]+command.VtxOffset].pos;
            rect.x=(std::min)(rect.x,position.x);rect.y=(std::min)(rect.y,position.y);
            rect.z=(std::max)(rect.z,position.x);rect.w=(std::max)(rect.w,position.y);
        }
        require3D(rect.z>rect.x&&rect.w>rect.y,"Slice has an empty ImGui image rectangle");return rect;
    }
    throw std::runtime_error("Rendered slice texture never reached ImGui draw commands");
}

// Read a point away from crosshairs/grids and prove that the on-screen material is opaque and colored.
void requireVisibleSlice3D(const Seis3D::Seis3D_State& s) {
    const auto rect=visibleSliceRect3D(s.gl3D.displayTextures3D[0]);
    unsigned char actual[4]{};glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    glReadPixels(int(rect.x+(rect.z-rect.x)*.13f),899-int(rect.y+(rect.w-rect.y)*.83f),1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual);
    const auto expected=pixel3D(s,int(s.gl3D.layout3D.sliceWidth3D[0]*.13f),int(s.gl3D.layout3D.sliceHeight3D[0]*.83f));
    require3D(expected[3]==255,"Slice offscreen target is transparent");
    for(int channel=0;channel<3;++channel) require3D(std::abs(int(actual[channel])-int(expected[channel]))<14,"ImGui slice differs from its rendered material texture");
}

// Save the actual GL backbuffer for optional visual review without displaying a native window.
void saveFrame(const std::string& name) {
    std::vector<unsigned char> pixels(1280*900*3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,1280,900,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream image(name+".ppm",std::ios::binary);image<<"P6\n1280 900\n255\n";
    for(int row=899;row>=0;--row) image.write(reinterpret_cast<const char*>(pixels.data()+row*1280*3),1280*3);
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
    ImFontConfig fontConfig;fontConfig.SizePixels=21;
    const auto baseFont=resolveResourcePath("font/consola.ttf");
    if(std::filesystem::exists(baseFont)) ImGui::GetIO().Fonts->AddFontFromFileTTF(pathToUtf8(baseFont).c_str(),21,&fontConfig);
    else ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
    const auto chineseFont=resolveResourcePath("font/msyh.ttc");
    if(std::filesystem::exists(chineseFont)) {
        ImFontConfig merge;merge.MergeMode=true;merge.PixelSnapH=true;
        ImGui::GetIO().Fonts->AddFontFromFileTTF(pathToUtf8(chineseFont).c_str(),21,&merge,ImGui::GetIO().Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 430");
    int result=0;
    try {
        GpuInfo deviceInfo{};cudaDeviceProp deviceProperties{};cudaGetDeviceProperties(&deviceProperties,devices[0]);
        std::snprintf(deviceInfo.name,sizeof(deviceInfo.name),"%s",deviceProperties.name);
        deviceInfo.success=true;deviceInfo.totalMem=deviceProperties.totalGlobalMem;
        deviceInfo.computeCapabilityMajor=deviceProperties.major;deviceInfo.computeCapabilityMinor=deviceProperties.minor;
        deviceInfo.multiProcessorCount=deviceProperties.multiProcessorCount;
        for(int theme=0;theme<int(AppUi::ThemeId::Count);++theme) {
            AppUi::setTheme(AppUi::ThemeId(theme),false);
            const auto& p=AppUi::palette();
            require3D(std::abs(p.text.x-p.background.x)>.45f,"Theme text lacks contrast");
            if(p.light) {
                {AppUi::LegacyContrastScope legacy(true);
                    require3D(ImGui::GetStyleColorVec4(ImGuiCol_Text).x>.8f,"Legacy dark canvas lost readable text");}
                require3D(AppUi::currentTheme()==AppUi::ThemeId(theme)
                    && ImGui::GetStyleColorVec4(ImGuiCol_Text).x==p.text.x,"Legacy scope changed selected theme");
            }
            for(int frame=0;frame<3;++frame) {
                ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
                require3D(drawHomeScreen(deviceInfo,1280,900)==AppScreen::Intro0,"Home changed route without input");
                ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
            if(argc>1) saveFrame("home-theme-"+std::to_string(theme));
        }
        // Mouse activation verifies that each visible launch card routes to its intended sandbox.
        for(int dimension=0;dimension<2;++dimension) {
            ImGui::GetIO().AddMousePosEvent(dimension==0?350.0f:950.0f,400.0f);
            for(int phase=0;phase<2;++phase) {
                ImGui::GetIO().AddMouseButtonEvent(0,phase==0);
                ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
                const auto route=drawHomeScreen(deviceInfo,1280,900);
                if(phase==1) require3D(route==(dimension==0?AppScreen::SeisSim_GPU:AppScreen::SeisSim_3D),"Home launch card routed incorrectly");
                ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
        }
        ImGui::GetIO().AddMousePosEvent(-100,-100);
        AppUi::setTheme(AppUi::ThemeId::CudaGreen,false);
        Seis3D::Seis3D_State s;
        s.materialBrightness3D=1;s.component3D=0;
        s.editContext3D.nx3D=33;s.editContext3D.ny3D=31;s.editContext3D.nz3D=29;s.editContext3D.pml3D=8;
        Seis3D::Detail3D::initializeSession3D(s,false);
        Seis3D::Detail3D::configureReceivers3D(s);
        require3D(s.receivers3D.size()==100 && s.gpu3D.receiverCount3D==100,"Default 10 x 10 acquisition failed");
        bool rejected=false;
        s.survey3D.receiverCountX3D=11;
        try {Seis3D::Detail3D::configureReceivers3D(s);} catch(const std::runtime_error&) {rejected=true;}
        require3D(rejected && s.gpu3D.receiverCount3D==100,"Over-limit geometry replaced valid receivers");
        s.survey3D.receiverCountX3D=10;
        std::vector<Seis3D::Receiver3D> tooMany(101);
        rejected=false;
        try {Seis3D::setReceivers3D(s.gpu3D,s.context3D,tooMany.data(),101);} catch(const std::invalid_argument&) {rejected=true;}
        require3D(rejected && s.gpu3D.receiverCount3D==100,"CUDA receiver API accepted 101 channels");
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
        require3D(s.gather3D.receivers3D.size()==100 && s.gather3D.samples3D.size()==100*64*3,"100-channel three-component gather size is wrong");
        for(int preset=0;preset<3;++preset) {
            s.layoutPreset3D=preset;s.resetLayout3D=true;
            AppUi::setTheme(preset==0?AppUi::ThemeId::CudaGreen:preset==1?AppUi::ThemeId::OceanBlue:AppUi::ThemeId::ScientificLight,false);
            for(int frame=0;frame<4;++frame) {
                ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
                Seis3D::renderScreen3D(s,1280,900);ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                require3D(!s.messageIsError3D,s.message3D.c_str());
            }
            if(preset==0) requireVisibleSlice3D(s);
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
        // Real mouse events cover the new canvas path and prevent zoom from breaking physical picking.
        s.layoutPreset3D=0;s.resetLayout3D=true;
        const auto drawFrame=[&]{
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
            Seis3D::renderScreen3D(s,1280,900);ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        };
        for(int i=0;i<3;++i) drawFrame();
        auto rect=visibleSliceRect3D(s.gl3D.displayTextures3D[0]);
        ImGui::GetIO().AddMousePosEvent(rect.x+(rect.z-rect.x)*.4f,rect.y+(rect.w-rect.y)*.45f);drawFrame();
        const auto beforePoint=s.hoverPoint3D;
        ImGui::GetIO().AddMouseWheelEvent(0,3);drawFrame();
        require3D(s.sliceCameras3D[0].zoom3D>1.5f&&s.sliceCameras3D[1].zoom3D==1,"Wheel zoom did not remain local to its plane");
        require3D(s.hoverPoint3D.x3D==beforePoint.x3D&&s.hoverPoint3D.z3D==beforePoint.z3D,"Pointer-anchored zoom changed the picked cell");
        const float oldCenter=s.sliceCameras3D[0].center3D[0];
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Middle,true);drawFrame();
        ImGui::GetIO().AddMousePosEvent(rect.x+(rect.z-rect.x)*.5f,rect.y+(rect.w-rect.y)*.45f);drawFrame();
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Middle,false);drawFrame();
        require3D(s.sliceCameras3D[0].center3D[0]<oldCenter,"Middle-button panning did not move the view");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_R,true);drawFrame();ImGui::GetIO().AddKeyEvent(ImGuiKey_R,false);drawFrame();
        require3D(s.sliceCameras3D[0].zoom3D==1&&s.sliceCameras3D[0].center3D[0]==.5f,"R did not reset the slice camera");
        ImGui::GetIO().AddMousePosEvent(-100,-100);
        // Ordinary Run produces real monitor samples and completion can replay a fresh wavelet.
        Seis3D::Detail3D::restartSimulation3D(s,true);s.maximumSteps3D=128;
        for(int i=0;i<16;++i) Seis3D::Detail3D::advanceSimulation3D(s);
        require3D(!s.running3D&&s.liveTrace3D.count3D==16,"Run did not populate the live monitor");
        float livePeak=0;for(float value:s.liveTrace3D.samples3D) livePeak=(std::max)(livePeak,std::abs(value));
        require3D(livePeak>1e-10f,"Live receiver monitor has no physical signal");
        Seis3D::Detail3D::toggleSimulation3D(s);
        require3D(s.running3D&&s.step3D==0&&s.liveTrace3D.count3D==0,"Completed run did not restart its source wavelet");
        s.maximumSteps3D=10000;s.stepsPerFrame3D=1;
        for(int i=0;i<520;++i) Seis3D::Detail3D::advanceSimulation3D(s);
        require3D(s.liveTrace3D.count3D==512&&s.liveTrace3D.samples3D.size()==512*100*3,"Live monitor exceeded its bounded capacity");
        const int last=(s.liveTrace3D.next3D+511)%512;
        require3D(s.liveTrace3D.times3D[last]>s.liveTrace3D.times3D[s.liveTrace3D.next3D],"Wrapped live sample times lost chronological order");
        require3D(s.gather3D.sampleCount3D==64,"Live monitoring overwrote the frozen capture");
        s.running3D=false;
        for(int preset=0;preset<4;++preset) {
            Seis3D::Detail3D::applyVisualPreset3D(s,preset);drawFrame();
            require3D(!s.messageIsError3D,s.message3D.c_str());
        }
        require3D(glGetError()==GL_NO_ERROR,"OpenGL validation error");
        Seis3D::releaseSandbox3D(s);
        require3D(!s.ready3D&&!s.gl3D.interop3D&&!s.gpu3D.stream3D,"Cold release left resources alive");
        std::puts("PASS: opaque on-screen slices, zoom/pan/reset, live monitor/replay, visual presets, 8 themes, 100-channel capture, RG interop, dock presets, cold release");
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());result=1;}
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImPlot::DestroyContext();ImGui::DestroyContext();
    glfwDestroyWindow(window);glfwTerminate();return result;
}
