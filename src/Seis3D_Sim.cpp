#include "Seis3D/Internal3D.h"
#include "Seis3D_Sim.h"
#include "Cuda_Seis3D.cuh"
#include "ResourcePaths.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Seis3D {
namespace Detail3D {



// Decode UI UTF-8 paths using C++20 char8_t rather than the deprecated u8path overload.
std::filesystem::path uiPath3D(const char* text) {
    std::u8string encoded(std::strlen(text),u8'\0');
    std::memcpy(encoded.data(),text,encoded.size());
    return std::filesystem::path(encoded);
}

// Report CUDA errors through the 3-D panel, preserving the application and 2-D route.
void requireCuda3D(cudaError_t result, const char* operation) {
    if (result != cudaSuccess) throw std::runtime_error(std::string(operation)+": "+cudaGetErrorString(result));
}

// Return a unique, filesystem-safe session suffix without depending on a locale-specific date.
std::string sessionSuffix3D() {
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Fit editable survey defaults to a newly selected physical grid.
void fitSurvey3D(Seis3D_State& s) {
    auto& survey=s.survey3D; const auto& c=s.context3D;
    survey.receiverCountX3D=(std::clamp)(survey.receiverCountX3D,1,(std::min)(64,c.nx3D-2));
    survey.receiverCountY3D=(std::clamp)(survey.receiverCountY3D,1,(std::min)(64,c.ny3D-2));
    survey.shotCountX3D=(std::clamp)(survey.shotCountX3D,1,100);
    survey.shotCountY3D=(std::clamp)(survey.shotCountY3D,1,100);
    survey.receiverOrigin3D[0]=survey.receiverOrigin3D[1]=1; survey.receiverOrigin3D[2]=0;
    survey.receiverSpacing3D[0]=(std::max)(1,(c.nx3D-3)/(std::max)(1,survey.receiverCountX3D-1));
    survey.receiverSpacing3D[1]=(std::max)(1,(c.ny3D-3)/(std::max)(1,survey.receiverCountY3D-1));
    survey.shotOrigin3D[0]=c.nx3D/4; survey.shotOrigin3D[1]=c.ny3D/4; survey.shotOrigin3D[2]=(std::max)(2,c.nz3D/8);
    survey.shotSpacing3D[0]=(std::max)(1,(c.nx3D-2-survey.shotOrigin3D[0])/(std::max)(1,survey.shotCountX3D-1));
    survey.shotSpacing3D[1]=(std::max)(1,(c.ny3D-2-survey.shotOrigin3D[1])/(std::max)(1,survey.shotCountY3D-1));
}

// Replace the complete session only after CPU validation, and roll back GPU/GL partial failures.
void initializeSession3D(Seis3D_State& s,bool imported) {
    auto candidate=s.editContext3D;
    const auto path=uiPath3D(s.modelPath3D);
    if(imported) candidate=inspectModel3D(path,candidate);
    else { if(candidate.modelKind3D==ModelKind3D::Imported) candidate.modelKind3D=ModelKind3D::Uniform; validateContext3D(candidate); }
    if(estimateMemory3D(candidate).totalBytes3D>memoryLimit3D) throw std::runtime_error("Grid exceeds the 4 GiB sandbox budget.");
    releaseSandbox3D(s); s.context3D=candidate; s.editContext3D=candidate;
    try {
        initializeGpu3D(s.gpu3D,s.context3D);
        if(imported) importModel3D(s,path);
        initializeGraphics3D(s);
        s.source3D.x3D=candidate.nx3D/2; s.source3D.y3D=candidate.ny3D/2; s.source3D.z3D=(std::max)(2,candidate.nz3D/4);
        s.slice3D[0]=s.source3D.x3D; s.slice3D[1]=s.source3D.y3D; s.slice3D[2]=s.source3D.z3D;
        fitSurvey3D(s); s.receivers3D.clear(); s.step3D=0; s.viewMaximum3D=1; s.hoverValid3D=false;
        s.ready3D=true; s.dirty3D=true; s.message3D="3D session ready. Safe dt synchronized from model maximum Vp.";
    } catch(...) { releaseSandbox3D(s); throw; }
}

// Execute a bounded batch so the large-grid solver cannot queue unbounded work ahead of the UI.
void advanceSimulation3D(Seis3D_State& s) {
    if(!s.ready3D||(!s.running3D&&!s.singleStep3D)) return;
    requireCuda3D(cudaEventRecord(s.gpu3D.startEvent3D,s.gpu3D.stream3D),"start 3D timer");
    const int batch=s.singleStep3D?1:(std::clamp)(s.stepsPerFrame3D,1,8); s.singleStep3D=false;
    for(int i=0;i<batch;++i) {
        const Source3D& source=s.recording3D?s.gather3D.source3D:s.source3D;
        runStep3D(s.gpu3D,s.context3D,source,s.step3D); ++s.step3D;
        if(s.recording3D&&s.step3D%std::uint64_t(s.gather3D.stride3D)==0) {
            const auto samples=sampleReceivers3D(s.gpu3D,s.context3D);
            const std::size_t count=s.gather3D.receivers3D.size()*3;
            std::copy_n(samples,count,s.gather3D.samples3D.data()+std::size_t(s.gather3D.sampleCount3D)*count);
            ++s.gather3D.sampleCount3D;
            if(s.gather3D.sampleCount3D==s.gather3D.targetSamples3D) {
                s.recording3D=false; s.running3D=false;
                if(s.sequence3D) {
                    exportGather3D(s,s.sequenceDirectory3D);
                    ++s.sequenceIndex3D;
                    if(s.sequenceIndex3D<s.survey3D.shotCountX3D*s.survey3D.shotCountY3D) {
                        positionSequenceShot3D(s); startGather3D(s,s.sequenceIndex3D+1);
                    } else { s.sequence3D=false; s.message3D="3D multi-line shot sequence completed and exported."; }
                } else s.message3D="3D capture complete. Export the three-component shot gather.";
                break;
            }
        }
        if(!s.recording3D&&s.step3D>=std::uint64_t((std::clamp)(s.maximumSteps3D,1,1000000))) { s.running3D=false; break; }
    }
    requireCuda3D(cudaEventRecord(s.gpu3D.stopEvent3D,s.gpu3D.stream3D),"stop 3D timer");
    requireCuda3D(cudaEventSynchronize(s.gpu3D.stopEvent3D),"finish 3D compute batch");
    requireCuda3D(cudaEventElapsedTime(&s.computeMs3D,s.gpu3D.startEvent3D,s.gpu3D.stopEvent3D),"read 3D compute time");
}

} // namespace

using namespace Detail3D;

// Initialize CPU-only state and default paths next to the deployed program, inside outer out.
Seis3D_State::Seis3D_State() {
    const auto directory=getExecutableDirectory()/"data_Seis3D";
    const auto model=pathToUtf8(directory/"model.s3dm"); const auto output=pathToUtf8(directory);
    std::snprintf(modelPath3D,sizeof(modelPath3D),"%s",model.c_str());
    std::snprintf(exportDirectory3D,sizeof(exportDirectory3D),"%s",output.c_str());
}

// Release owned resources while the owning application still has its GL context current.
Seis3D_State::~Seis3D_State() { releaseSandbox3D(*this); }

// Unregister CUDA-GL interop before deleting GL storage, then destroy only the 3-D stream/resources.
void releaseSandbox3D(Seis3D_State& s) noexcept {
    if(s.gpu3D.stream3D) cudaStreamSynchronize(s.gpu3D.stream3D);
    auto& g=s.gl3D;
    if(g.stagingBuffer3D||g.vao3D) glFinish(); // Cold switches wait for the last GL reader before reclaiming VRAM.
    if(g.interop3D) cudaGraphicsUnregisterResource(g.interop3D);
    if(g.stagingBuffer3D) glDeleteBuffers(1,&g.stagingBuffer3D);
    if(g.vao3D) glDeleteVertexArrays(1,&g.vao3D);
    if(g.sliceProgram3D) glDeleteProgram(g.sliceProgram3D);
    if(g.volumeProgram3D) glDeleteProgram(g.volumeProgram3D);
    for(int p=0;p<3;++p) {
        if(g.scalarTextures3D[p]) glDeleteTextures(1,&g.scalarTextures3D[p]);
        if(g.displayTextures3D[p]) glDeleteTextures(1,&g.displayTextures3D[p]);
        if(g.framebuffers3D[p]) glDeleteFramebuffers(1,&g.framebuffers3D[p]);
    }
    if(g.volumeTexture3D) glDeleteTextures(1,&g.volumeTexture3D);
    if(g.volumeDisplayTexture3D) glDeleteTextures(1,&g.volumeDisplayTexture3D);
    if(g.volumeFramebuffer3D) glDeleteFramebuffers(1,&g.volumeFramebuffer3D);
    g=GLHandles3D{}; releaseGpu3D(s.gpu3D);
    s.ready3D=false; s.running3D=false; s.recording3D=false; s.sequence3D=false; s.singleStep3D=false; s.hoverValid3D=false;
}

// Draw the 3-D launch card using schematic geometry only; no solver resources are allocated here.
int drawIntroCard3D(float x,float y,float width,float height,float scale,const char* deviceName,double vramGiB) {
    auto* draw=ImGui::GetWindowDrawList(); const ImVec2 origin(x,y),end(x+width,y+height);
    ImGui::SetCursorScreenPos(origin);ImGui::InvisibleButton("##launchSandbox3D",ImVec2(width,height-90*scale));
    const bool clicked=ImGui::IsItemClicked(),hovered=ImGui::IsItemHovered();
    draw->AddRectFilled(origin,end,IM_COL32(17,29,36,244),9*scale);
    draw->AddRect(origin,end,hovered?IM_COL32(118,235,93,230):IM_COL32(112,205,119,155),9*scale,0,1.35f*scale);
    draw->AddRectFilled(origin,ImVec2(x+width,y+4*scale),IM_COL32(101,207,110,205),2*scale);
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+28*scale));ImGui::SetWindowFontScale(0.84f*scale);
    ImGui::TextColored(ImVec4(0.58f,0.88f,0.48f,1),"3D SPATIAL EXPLORATION SANDBOX");
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+72*scale));ImGui::SetWindowFontScale(1.75f*scale);
    ImGui::TextColored(ImVec4(0.96f,0.98f,1,1),"三维空间探索沙盒");
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+124*scale));ImGui::SetWindowFontScale(0.96f*scale);
    ImGui::PushTextWrapPos(x+width-34*scale);
    ImGui::TextColored(ImVec4(0.69f,0.76f,0.84f,1),"三维弹性波正演 · 正交切片 · 体绘制\n地表接收网格 / 多测线放炮 / 3D SEG-Y");ImGui::PopTextWrapPos();
    const ImVec2 center(x+width*0.5f,y+height*0.52f+40*scale);
    const float extent=(std::min)(width*0.21f,height*0.18f);
    // Project a wire box and three orthogonal schematic planes with depth grid lines.
    const auto project=[&](float a,float b,float c) {return ImVec2(center.x+extent*(a+0.5f*b),center.y+extent*(0.3f*b-c));};
    for(int axis=0;axis<3;++axis) for(int sideA=-1;sideA<=1;sideA+=2) for(int sideB=-1;sideB<=1;sideB+=2) {
        float a[3]={float(sideA),float(sideB),-1},b[3]={float(sideA),float(sideB),1};
        if(axis==0) {a[0]=-1;b[0]=1;a[2]=b[2]=float(sideA);a[1]=b[1]=float(sideB);}
        if(axis==1) {a[1]=-1;b[1]=1;a[0]=b[0]=float(sideA);a[2]=b[2]=float(sideB);}
        draw->AddLine(project(a[0],a[1],a[2]),project(b[0],b[1],b[2]),IM_COL32(114,210,119,180),1.2f*scale);
    }
    for(int i=-3;i<=3;++i) {
        const float q=i/4.0f;
        draw->AddLine(project(-1,q,0),project(1,q,0),IM_COL32(95,183,113,60));
        draw->AddLine(project(q,0,-1),project(q,0,1),IM_COL32(95,183,113,60));
    }
    for(int ring=0;ring<4;++ring) {
        const float phase=std::fmod(float(ImGui::GetTime())*0.24f+ring*0.25f,1.0f);
        ImVec2 points[65];
        for(int i=0;i<=64;++i) {const float angle=i*6.2831853f/64;points[i]=project(phase*std::cos(angle),phase*std::sin(angle),0);}
        draw->AddPolyline(points,65,IM_COL32(126,239,112,int(140*(1-phase)+20)),0,1.4f*scale);
    }
    draw->AddCircleFilled(center,3.5f*scale,IM_COL32(245,194,96,240));
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+height-130*scale));ImGui::SetWindowFontScale(0.8f*scale);
    ImGui::PushTextWrapPos(x+width-30*scale);ImGui::TextColored(ImVec4(0.53f,0.69f,0.6f,1),"%s / %.1f GiB",deviceName,vramGiB);ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+height-82*scale));
    ImGui::InvisibleButton("##launchSandboxFooter3D",ImVec2((std::max)(1.0f,width-210*scale),45*scale));
    const bool footerClicked=ImGui::IsItemClicked();
    ImGui::SetCursorScreenPos(ImVec2(x+34*scale,y+height-72*scale));ImGui::SetWindowFontScale(0.9f*scale);
    ImGui::TextColored(ImVec4(0.62f,0.91f,0.52f,1),"进入三维工作区    →");
    ImGui::SetCursorScreenPos(ImVec2(x+width-156*scale,y+height-68*scale));ImGui::SetWindowFontScale(0.8f*scale);
    const bool device=ImGui::Button("设备信息",ImVec2(124*scale,30*scale));
    ImGui::SetWindowFontScale(1);
    return device?2:(clicked||footerClicked)?1:0;
}

} // namespace Seis3D
