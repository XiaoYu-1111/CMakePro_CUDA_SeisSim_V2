#include "Internal3D.h"
#include "Visual3D.h"
#include "imgui_internal.h"
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

// Place labels above full-width controls so docking to a narrow column never clips their names.
std::string fieldLabel3D(const char* label) {
    ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
    return std::string("##")+label;
}

// Show bounded grid presets and explicit material/CFL controls without mutating a live model.
void drawSimulationControls3D(Seis3D_State& s) {
    auto& c=s.editContext3D;
    ImGui::BeginDisabled(s.recording3D);
    if(ImGui::Button(s.ready3D?"Rebuild 3D session":"Initialize 3D",ImVec2(-1,0))) performAction3D(s,[&]{initializeSession3D(s,false);});
    ImGui::TextUnformatted("Grid presets");
    if(ImGui::BeginTable("GridPresets3D",2,ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();if(ImGui::Button("96 cubed",ImVec2(-1,0))) c.nx3D=c.ny3D=c.nz3D=96;
        ImGui::TableNextColumn();if(ImGui::Button("160 cubed",ImVec2(-1,0))) c.nx3D=c.ny3D=c.nz3D=160;
        ImGui::TableNextColumn();if(ImGui::Button("300 cubed",ImVec2(-1,0))) c.nx3D=c.ny3D=c.nz3D=300;
        ImGui::TableNextColumn();if(ImGui::Button("400/400/300",ImVec2(-1,0))) {c.nx3D=c.ny3D=400;c.nz3D=300;}
        ImGui::EndTable();
    }
    int sizes[3]={c.nx3D,c.ny3D,c.nz3D};
    if(ImGui::InputInt3(fieldLabel3D("Nodes X/Y/Z").c_str(),sizes)) { c.nx3D=sizes[0];c.ny3D=sizes[1];c.nz3D=sizes[2]; }
    ImGui::InputFloat(fieldLabel3D("Grid spacing (m)").c_str(),&c.spacing3D,1,10,"%.3f");
    ImGui::SliderInt(fieldLabel3D("CPML cells").c_str(),&c.pml3D,8,40);
    ImGui::Checkbox("Traction-free top z=0",&c.freeSurface3D);
    int kind=int(c.modelKind3D); if(kind==3) kind=0;
    if(ImGui::Combo(fieldLabel3D("Procedural model").c_str(),&kind,"Uniform\0Two layers\0Sphere anomaly\0")) c.modelKind3D=ModelKind3D(kind);
    ImGui::InputFloat(fieldLabel3D("Vp (m/s)").c_str(),&c.vp3D,100,1000,"%.1f");
    ImGui::InputFloat(fieldLabel3D("Vs (m/s)").c_str(),&c.vs3D,100,1000,"%.1f");
    ImGui::InputFloat(fieldLabel3D("Density (kg/m3)").c_str(),&c.density3D,100,1000,"%.1f");
    if(ImGui::CollapsingHeader("Layer / sphere material")) {
        ImGui::InputFloat(fieldLabel3D("Secondary Vp").c_str(),&c.layerVp3D,100,1000,"%.1f");
        ImGui::InputFloat(fieldLabel3D("Secondary Vs").c_str(),&c.layerVs3D,100,1000,"%.1f");
        ImGui::InputFloat(fieldLabel3D("Secondary density").c_str(),&c.layerDensity3D,100,1000,"%.1f");
    }
    ImGui::InputFloat(fieldLabel3D("Ricker f0 (Hz)").c_str(),&c.frequency3D,1,10,"%.2f");
    ImGui::SliderFloat(fieldLabel3D("CFL safety factor").c_str(),&c.cfl3D,0.2f,0.9f,"%.2f");
    try {
        auto preview=c; validateContext3D(preview); const auto budget=estimateMemory3D(preview);
        ImGui::SeparatorText("MEMORY / STABILITY");
        ImGui::Text("Fields + materials: %.2f GiB",budget.fieldBytes3D/gibibyte3D);
        ImGui::Text("Compact CPML: %.2f GiB",budget.cpmlBytes3D/gibibyte3D);
        ImGui::TextColored(budget.totalBytes3D<=memoryLimit3D?ImVec4(0.55f,0.9f,0.38f,1):ImVec4(1,0.45f,0.3f,1),
            "Total estimate: %.2f / 4.00 GiB",budget.totalBytes3D/gibibyte3D);
        ImGui::Text("Safe dt: %.3f ms / Vp max %.0f",preview.dt3D*1000,preview.maxVp3D);
        ImGui::TextDisabled("Includes padding, staging and display textures.");
    } catch(const std::exception&) { ImGui::TextColored(ImVec4(1,0.55f,0.35f,1),"Correct invalid parameters before initializing."); }
    ImGui::EndDisabled();
    ImGui::Separator(); ImGui::SliderInt(fieldLabel3D("Steps per frame").c_str(),&s.stepsPerFrame3D,1,8);
    ImGui::SliderInt(fieldLabel3D("Stop after steps").c_str(),&s.maximumSteps3D,100,20000);
    ImGui::TextWrapped("8th-order staggered interior / 2nd-order time. Planar free-surface closure: 2nd order.");
}

// Configure a compact explosive or directed body-force source independently from the 2-D tools.
void drawSourceControls3D(Seis3D_State& s) {
    ImGui::BeginDisabled(!s.ready3D||s.recording3D);
    const auto& c=s.context3D;
    ImGui::SliderInt(fieldLabel3D("Source X").c_str(),&s.source3D.x3D,1,(std::max)(2,c.nx3D-2));
    ImGui::SliderInt(fieldLabel3D("Source Y").c_str(),&s.source3D.y3D,1,(std::max)(2,c.ny3D-2));
    ImGui::SliderInt(fieldLabel3D("Source depth Z").c_str(),&s.source3D.z3D,2,(std::max)(3,c.nz3D-2));
    int kind=int(s.source3D.kind3D);
    if(ImGui::Combo(fieldLabel3D("Source mechanism").c_str(),&kind,"Isotropic explosion\0Directed body force\0")) s.source3D.kind3D=SourceKind3D(kind);
    ImGui::SliderFloat(fieldLabel3D("Amplitude").c_str(),&s.source3D.amplitude3D,1e3f,1e12f,"%.3e",ImGuiSliderFlags_Logarithmic);
    ImGui::TextDisabled(s.source3D.kind3D==SourceKind3D::Explosion?"Amplitude unit: Pa/s":"Amplitude unit: N/m3");
    if(s.source3D.kind3D==SourceKind3D::Force) {
        ImGui::InputFloat3(fieldLabel3D("Direction XYZ").c_str(),s.source3D.direction3D);
        if(ImGui::Button("Normalize direction")) performAction3D(s,[&]{
            const auto d=s.source3D.direction3D; const float norm=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
            if(!std::isfinite(norm)||norm<1e-10f) throw std::runtime_error("Force direction must be finite and non-zero.");
            for(auto& value:s.source3D.direction3D) value/=norm;
        });
    }
    if(ImGui::Button("Center slices on source",ImVec2(-1,0))) {
        s.slice3D[0]=s.source3D.x3D;s.slice3D[1]=s.source3D.y3D;s.slice3D[2]=s.source3D.z3D;s.dirty3D=true;
    }
    if(ImGui::Button("Reset wavefield + fire",ImVec2(-1,0))) performAction3D(s,[&]{
        resetGpu3D(s.gpu3D,s.context3D);s.step3D=0;s.running3D=true;s.dirty3D=true;
    });
    ImGui::EndDisabled();
    ImGui::TextWrapped("A normalized 3 x 3 x 3 spatial source reduces grid-scale excitation. Wavelet delay is 1.2 / f0.");
}

// Provide streaming model I/O using SI Vp/Vs/rho, with no reuse of the 2-D import path.
void drawModelControls3D(Seis3D_State& s) {
    ImGui::TextWrapped(".s3dm: physical XYZ grid, SI Vp / Vs / rho. X is contiguous; padding is regenerated.");
    ImGui::InputText(fieldLabel3D("Model path").c_str(),s.modelPath3D,sizeof(s.modelPath3D));
    ImGui::BeginDisabled(s.recording3D);
    if(ImGui::Button("Validate + import 3D model",ImVec2(-1,0))) performAction3D(s,[&]{initializeSession3D(s,true);});
    ImGui::BeginDisabled(!s.ready3D);
    if(ImGui::Button("Export model to a new file",ImVec2(-1,0))) performAction3D(s,[&]{exportModel3D(s);});
    ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("The importer scans every material cell and computes maximum Vp before allocating GPU memory. Large models are transferred one depth plane at a time.");
    ImGui::TextDisabled("Exports preserve existing files; choose a new name.");
}

// Configure independent geological material, signed wave transfer functions and rendering cadence.
void drawViewControls3D(Seis3D_State& s) {
    ImGui::SeparatorText("COMPOSITION");
    ImGui::Combo(fieldLabel3D("Display").c_str(),&s.displayMode3D,"Model + wavefield\0Wavefield only\0Material only\0");
    if(ImGui::Combo(fieldLabel3D("Wave component").c_str(),&s.component3D,"Vz\0Vx\0Vy\0Velocity magnitude\0Pressure\0")) s.dirty3D=true;
    ImGui::Combo(fieldLabel3D("Wave palette").c_str(),&s.waveStyle3D,waveStyleNames3D);
    ImGui::Checkbox("Automatic common scale",&s.autoScale3D);
    if(!s.autoScale3D) ImGui::SliderFloat(fieldLabel3D("Amplitude scale").c_str(),&s.manualScale3D,1e-8f,1e10f,"%.3e",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Display gain").c_str(),&s.gain3D,0.1f,20.0f,"%.2f",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Wave opacity").c_str(),&s.waveOpacity3D,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Wave cutoff").c_str(),&s.waveThreshold3D,0,0.8f,"%.3f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("GEOLOGICAL MATERIAL");
    int material=s.materialComponent3D-5;
    if(ImGui::Combo(fieldLabel3D("Property").c_str(),&material,"Vp (m/s)\0Vs (m/s)\0Density (kg/m3)\0")) {s.materialComponent3D=material+5;s.dirty3D=true;}
    ImGui::Combo(fieldLabel3D("Model palette").c_str(),&s.modelStyle3D,"Dark Titanium\0Geological Map\0Grayscale Velocity\0WaveStyle Synced\0Scientific White\0");
    ImGui::Checkbox("Auto material range (visible data)",&s.materialRangeAuto3D);
    if(!s.materialRangeAuto3D) {
        ImGui::InputFloat2(fieldLabel3D("Range min / max").c_str(),s.manualMaterialRange3D,"%.1f");
        if(!std::isfinite(s.manualMaterialRange3D[0])||!std::isfinite(s.manualMaterialRange3D[1])) {s.manualMaterialRange3D[0]=0;s.manualMaterialRange3D[1]=1;}
        if(s.manualMaterialRange3D[1]<=s.manualMaterialRange3D[0]) ImGui::TextWrapped("Set maximum above minimum for material contrast.");
    }
    if(ImGui::Button("Freeze visible material range")) {
        std::copy_n(s.materialRange3D,2,s.manualMaterialRange3D);s.materialRangeAuto3D=false;
    }
    ImGui::Checkbox("Physical grid",&s.showGrid3D);ImGui::SameLine();ImGui::Checkbox("Crosshairs",&s.showCrosshair3D);
    ImGui::SliderFloat(fieldLabel3D("Grid opacity").c_str(),&s.gridOpacity3D,0,0.6f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("VOLUME / PERFORMANCE");
    if(ImGui::Checkbox("Auxiliary volume raymarch",&s.showVolume3D)) s.dirty3D=true;
    ImGui::SliderFloat(fieldLabel3D("Cloud opacity").c_str(),&s.volumeOpacity3D,0.1f,8.0f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Cloud cutoff").c_str(),&s.volumeThreshold3D,0.0f,0.9f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Volume model opacity").c_str(),&s.modelOpacity3D,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt(fieldLabel3D("Display every N steps").c_str(),&s.displayStride3D,1,16,"%d",ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Display cadence preserves dt and receiver sampling.");
    if(ImGui::Button("Reset orbit camera")) s.camera3D=VolumeCamera3D{};
    ImGui::TextWrapped("Left drag: orbit. Right drag / wheel: zoom. Middle drag: pan. Slice click: link positions.");
}

// Show the exact transfer palette and physical range separately from display opacity.
void drawLegend3D(Seis3D_State& s) {
    ImGui::SeparatorText("COLOR / PHYSICAL SCALE");
    const char* components[]={"Vz","Vx","Vy","|V|","Pressure"};
    const float scale=(s.autoScale3D?s.viewMaximum3D:s.manualScale3D)/(std::max)(s.gain3D,0.001f);
    if(s.displayMode3D!=2) {
        ImGui::Text("%s  [%s]",components[s.component3D],s.component3D==4?"Pa":"m/s");
        const auto start=ImGui::GetCursorScreenPos();
        const float width=(std::max)(1.0f,ImGui::GetContentRegionAvail().x);
        for(int i=0;i<64;++i) ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
            ImVec2(start.x+width*i/64,start.y),ImVec2(start.x+width*(i+1)/64,start.y+12),
            paletteColor3D(s.waveStyle3D,float(i)/64),paletteColor3D(s.waveStyle3D,float(i+1)/64),
            paletteColor3D(s.waveStyle3D,float(i+1)/64),paletteColor3D(s.waveStyle3D,float(i)/64));
        ImGui::Dummy(ImVec2(width,14));
        ImGui::Text("%.2e  ...  %.2e",s.component3D==3?0.0f:-scale,scale);
        ImGui::TextDisabled("Palette before transparency; common to all views.");
    }
    if(s.displayMode3D!=1) {
        const auto* range=s.materialRangeAuto3D?s.materialRange3D:s.manualMaterialRange3D;
        ImGui::Text("%s: %.1f .. %.1f %s",s.materialComponent3D==5?"Vp":s.materialComponent3D==6?"Vs":"Density",
            range[0],range[1],s.materialComponent3D==7?"kg/m3":"m/s");
    }
}

// Plot a bounded preview of the selected receiver, retaining exact frozen sample times.
void drawTracePreview3D(Seis3D_State& s) {
    const auto& g=s.gather3D;
    if(!g.sampleCount3D||g.receivers3D.empty()) return;
    s.selectedReceiver3D=(std::clamp)(s.selectedReceiver3D,0,int(g.receivers3D.size())-1);
    ImGui::SliderInt("Preview receiver",&s.selectedReceiver3D,0,int(g.receivers3D.size())-1);
    std::array<float,2048> times{},vx{},vy{},vz{};
    const int decimation=(std::max)(1,(g.sampleCount3D+2047)/2048);
    int count=0;
    for(int i=0;i<g.sampleCount3D;i+=decimation) {
        const auto offset=(std::size_t(i)*g.receivers3D.size()+s.selectedReceiver3D)*3;
        times[count]=float(g.firstSampleTime3D+i*g.sampleIntervalUs3D*1e-6);
        vx[count]=g.samples3D[offset];vy[count]=g.samples3D[offset+1];vz[count]=g.samples3D[offset+2];++count;
    }
    if(ImPlot::BeginPlot("3-C receiver",ImVec2(-1,(std::max)(70.0f,ImGui::GetContentRegionAvail().y)))) {
        ImPlot::SetupAxes("Time (s)","Velocity (m/s)",ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);
        ImPlot::PlotLine("Vx",times.data(),vx.data(),count); ImPlot::PlotLine("Vy",times.data(),vy.data(),count);
        ImPlot::PlotLine("Vz",times.data(),vz.data(),count); ImPlot::EndPlot();
    }
}

// Configure surface receiver bins and multi-line shots, locking geometry for each active capture.
void drawAcquisitionControls3D(Seis3D_State& s) {
    auto& q=s.survey3D;
    ImGui::BeginDisabled(!s.ready3D||s.recording3D);
    ImGui::SeparatorText("SURFACE RECEIVER MESH");
    ImGui::InputInt(fieldLabel3D("Receiver count X").c_str(),&q.receiverCountX3D); ImGui::InputInt(fieldLabel3D("Receiver count Y").c_str(),&q.receiverCountY3D);
    ImGui::InputInt3(fieldLabel3D("Receiver origin XYZ").c_str(),q.receiverOrigin3D); ImGui::InputInt2(fieldLabel3D("Receiver spacing XY").c_str(),q.receiverSpacing3D);
    if(ImGui::Button("Fit spread to model")) { fitSurvey3D(s); }
    ImGui::SameLine(); if(ImGui::Button("Deploy mesh")) performAction3D(s,[&]{configureReceivers3D(s);});
    ImGui::InputDouble(fieldLabel3D("World origin X (m)").c_str(),&q.worldOrigin3D[0],0,0,"%.3f");
    ImGui::InputDouble(fieldLabel3D("World origin Y (m)").c_str(),&q.worldOrigin3D[1],0,0,"%.3f");
    ImGui::InputFloat2(fieldLabel3D("CMP bin size XY (m)").c_str(),q.cmpBinSize3D,"%.2f");
    ImGui::InputInt(fieldLabel3D("Samples per shot").c_str(),&s.targetSamples3D); ImGui::InputInt(fieldLabel3D("Sample stride (steps)").c_str(),&s.samplingStride3D);
    ImGui::Text("Actual sample interval: %.3f ms",s.samplingStride3D*s.context3D.dt3D*1000);
    ImGui::InputText(fieldLabel3D("Export folder").c_str(),s.exportDirectory3D,sizeof(s.exportDirectory3D));
    if(ImGui::Button("Capture current source",ImVec2(-1,0))) performAction3D(s,[&]{s.sequence3D=false;startGather3D(s,1);});
    ImGui::SeparatorText("MULTI-LINE SOURCE SPREAD");
    ImGui::InputInt(fieldLabel3D("Shots per line").c_str(),&q.shotCountX3D); ImGui::InputInt(fieldLabel3D("Source line count").c_str(),&q.shotCountY3D);
    ImGui::InputInt3(fieldLabel3D("First shot XYZ").c_str(),q.shotOrigin3D); ImGui::InputInt2(fieldLabel3D("Shot spacing XY").c_str(),q.shotSpacing3D);
    if(ImGui::Button("Start spread + auto export",ImVec2(-1,0))) performAction3D(s,[&]{
        if(!s.exportDirectory3D[0]) throw std::runtime_error("Choose an export folder.");
        s.sequenceIndex3D=0;positionSequenceShot3D(s);
        s.sequenceDirectory3D=uiPath3D(s.exportDirectory3D)/("survey_"+sessionSuffix3D());
        std::filesystem::create_directories(s.sequenceDirectory3D);
        startGather3D(s,1);s.sequence3D=true;
    });
    ImGui::EndDisabled();
    if(s.recording3D) {
        ImGui::ProgressBar(float(s.gather3D.sampleCount3D)/s.gather3D.targetSamples3D,ImVec2(-1,0));
        ImGui::Text("Shot %d / samples %d of %d",s.gather3D.shotId3D,s.gather3D.sampleCount3D,s.gather3D.targetSamples3D);
        if(ImGui::Button("Stop capture / spread")) { s.recording3D=false;s.sequence3D=false;s.running3D=false;s.message3D="Capture stopped; partial gather retained."; }
    }
    ImGui::BeginDisabled(s.recording3D||s.gather3D.sampleCount3D==0);
    if(ImGui::Button("Export captured 3-C SEG-Y",ImVec2(-1,0))) performAction3D(s,[&]{
        if(!s.exportDirectory3D[0]) throw std::runtime_error("Choose an export folder.");
        exportGather3D(s,uiPath3D(s.exportDirectory3D)/("capture_"+sessionSuffix3D()));
    });
    ImGui::EndDisabled();
}

// Fit a physical slice into a panel without stretching its spatial aspect ratio.
ImVec2 fittedImageSize3D(float ratio,ImVec2 available) {
    const float width=(std::max)(1.0f,available.x),height=(std::max)(1.0f,available.y);
    return width/height>ratio ? ImVec2(height*ratio,height) : ImVec2(width,width/ratio);
}

// Draw linked crosshairs and physical-coordinate source/receiver overlays for a single plane.
void drawSliceOverlay3D(Seis3D_State& s,int plane,ImVec2 origin,ImVec2 size) {
    const auto& c=s.context3D;
    const int width=plane==1?c.ny3D:c.nx3D, height=plane==2?c.ny3D:c.nz3D;
    const int a=plane==1?s.slice3D[1]:s.slice3D[0],b=plane==2?s.slice3D[1]:s.slice3D[2];
    auto* draw=ImGui::GetWindowDrawList();
    const float x=origin.x+(a+0.5f)*size.x/width,y=origin.y+(b+0.5f)*size.y/height;
    if(s.showCrosshair3D) draw->AddLine(ImVec2(x,origin.y),ImVec2(x,origin.y+size.y),IM_COL32(149,221,146,100));
    if(s.showCrosshair3D) draw->AddLine(ImVec2(origin.x,y),ImVec2(origin.x+size.x,y),IM_COL32(149,221,146,100));
    if(s.showGrid3D) {
        const ImU32 gridColor=ImGui::ColorConvertFloat4ToU32(ImVec4(.7f,.83f,.82f,s.gridOpacity3D));
        for(int i=1;i<5;++i) {
            const float u=origin.x+size.x*i/5, v=origin.y+size.y*i/5;
            draw->AddLine(ImVec2(u,origin.y),ImVec2(u,origin.y+size.y),gridColor);
            draw->AddLine(ImVec2(origin.x,v),ImVec2(origin.x+size.x,v),gridColor);
        }
    }
    // Project only markers that intersect this plane, preventing misleading 3-D positions.
    const auto project=[&](int px,int py,int pz,ImU32 color,float radius) {
        const bool intersects=plane==0?std::abs(py-s.slice3D[1])<=1:plane==1?std::abs(px-s.slice3D[0])<=1:std::abs(pz-s.slice3D[2])<=1;
        if(!intersects) return;
        const int u=plane==1?py:px,v=plane==2?py:pz;
        draw->AddCircleFilled(ImVec2(origin.x+(u+0.5f)*size.x/width,origin.y+(v+0.5f)*size.y/height),radius,color);
    };
    project(s.source3D.x3D,s.source3D.y3D,s.source3D.z3D,IM_COL32(255,188,69,255),4);
    for(const auto& receiver:s.receivers3D) project(receiver.x3D,receiver.y3D,receiver.z3D,IM_COL32(90,218,247,220),2);
    draw->AddRect(origin,ImVec2(origin.x+size.x,origin.y+size.y),IM_COL32(92,175,102,180),2);
}

// Draw one linked full-resolution slice with grid picking and material reverse lookup.
void drawSlicePanel3D(Seis3D_State& s,int plane) {
    ImGui::PushID(plane); ImGui::BeginChild("slicePanel",ImVec2(0,0),ImGuiChildFlags_None);
    if(!s.ready3D) { ImGui::TextWrapped("Initialize a 3D model to inspect this plane.");ImGui::EndChild();ImGui::PopID();return; }
    const int axis=plane==0?1:plane==1?0:2;
    const int count=axis==0?s.context3D.nx3D:axis==1?s.context3D.ny3D:s.context3D.nz3D;
    ImGui::SetNextItemWidth(-1);
    if(ImGui::SliderInt("##position",s.slice3D+axis,0,count-1,"%d",ImGuiSliderFlags_AlwaysClamp)) s.dirty3D=true;
    ImGui::TextDisabled("%c = %.0f m | span %.0f x %.0f m",'X'+axis,
        s.slice3D[axis]*s.context3D.spacing3D,
        (s.gl3D.layout3D.sliceWidth3D[plane]-1)*s.context3D.spacing3D,
        (s.gl3D.layout3D.sliceHeight3D[plane]-1)*s.context3D.spacing3D);
    const int width=s.gl3D.layout3D.sliceWidth3D[plane],height=s.gl3D.layout3D.sliceHeight3D[plane];
    const ImVec2 size=fittedImageSize3D(float(width)/height,ImGui::GetContentRegionAvail());
    const auto available=ImGui::GetContentRegionAvail();
    const auto cursor=ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x+(available.x-size.x)*.5f,cursor.y+(available.y-size.y)*.5f));
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    ImGui::Image(ImTextureID(s.gl3D.displayTextures3D[plane]),size,ImVec2(0,0),ImVec2(1,1));
    drawSliceOverlay3D(s,plane,origin,size);
    if(ImGui::IsItemHovered()) {
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        const int u=(std::clamp)(int((mouse.x-origin.x)*width/size.x),0,width-1);
        const int v=(std::clamp)(int((mouse.y-origin.y)*height/size.y),0,height-1);
        const Receiver3D point=plane==0?Receiver3D{u,s.slice3D[1],v}:plane==1?Receiver3D{s.slice3D[0],u,v}:Receiver3D{u,v,s.slice3D[2]};
        if(!s.hoverValid3D||point.x3D!=s.hoverPoint3D.x3D||point.y3D!=s.hoverPoint3D.y3D||point.z3D!=s.hoverPoint3D.z3D)
            performAction3D(s,[&]{probeMaterial3D(s.gpu3D,s.context3D,point,s.hoverMaterial3D);});
        s.hoverPoint3D=point;s.hoverValid3D=true;
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            s.slice3D[0]=point.x3D;s.slice3D[1]=point.y3D;s.slice3D[2]=point.z3D;s.dirty3D=true;
        }
    }
    ImGui::EndChild();ImGui::PopID();
}

// Draw the bounded raymarching exploration view and own all orbit-camera gestures locally.
void drawVolumePanel3D(Seis3D_State& s) {
    ImGui::BeginChild("volumePanel3D",ImVec2(0,0),ImGuiChildFlags_None);
    if(!s.ready3D||!s.showVolume3D) {
        ImGui::TextWrapped("Enable the auxiliary volume in Slice View. The three slices always use full-resolution solver data.");
    } else {
        ImGui::TextDisabled("LMB orbit / RMB zoom / MMB pan");
        const auto available=ImGui::GetContentRegionAvail();
        const auto size=fittedImageSize3D(1,available);
        const auto cursor=ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor.x+(available.x-size.x)*.5f,cursor.y+(available.y-size.y)*.5f));
        ImGui::Image(ImTextureID(s.gl3D.volumeDisplayTexture3D),size,ImVec2(0,1),ImVec2(1,0));
        if(ImGui::IsItemHovered()) {
            const auto& io=ImGui::GetIO();
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {s.camera3D.yaw3D-=io.MouseDelta.x*0.008f;s.camera3D.pitch3D+=io.MouseDelta.y*0.008f;}
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Right)) s.camera3D.distance3D*=std::exp(io.MouseDelta.y*0.01f);
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {s.camera3D.pan3D[0]-=io.MouseDelta.x*0.002f;s.camera3D.pan3D[1]+=io.MouseDelta.y*0.002f;}
            s.camera3D.distance3D*=std::exp(-io.MouseWheel*0.15f);
            s.camera3D.distance3D=(std::clamp)(s.camera3D.distance3D,1.0f,8.0f);
            s.camera3D.pitch3D=(std::clamp)(s.camera3D.pitch3D,-1.45f,1.45f);
        }
    }
    ImGui::EndChild();
}

} // namespace

using namespace Detail3D;

// Stable dock names keep saved layouts compatible across mode switches and application restarts.
static constexpr const char* panelNames3D[]={"Simulation controls###Seis3D.Controls","Display / materials###Seis3D.Display",
    "Inline X-Z###Seis3D.Inline","Crossline Y-Z###Seis3D.Crossline","Depth X-Y###Seis3D.Depth",
    "Spatial volume###Seis3D.Volume","Receiver traces###Seis3D.Traces"};

// Seed the dock tree only on first use or explicit preset/reset; dragging otherwise owns the layout.
void buildDockLayout3D(Seis3D_State& s,ImGuiID dockId,ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId,ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId,size);
    ImGuiID center=dockId;
    ImGuiID controls=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,(std::clamp)(360.0f/size.x,.22f,.38f),nullptr,&center);
    ImGui::DockBuilderDockWindow(panelNames3D[0],controls);
    ImGui::DockBuilderDockWindow(panelNames3D[1],controls);
    if(s.layoutPreset3D==2) {
        ImGui::DockBuilderDockWindow(panelNames3D[6],controls);
        for(int i=2;i<6;++i) ImGui::DockBuilderDockWindow(panelNames3D[i],center);
    } else {
        ImGuiID traces=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.22f,nullptr,&center);
        ImGui::DockBuilderDockWindow(panelNames3D[6],traces);
        ImGuiID left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,s.layoutPreset3D==1?.38f:.5f,nullptr,&center);
        if(s.layoutPreset3D==1) {
            for(int i=2;i<5;++i) ImGui::DockBuilderDockWindow(panelNames3D[i],left);
            ImGui::DockBuilderDockWindow(panelNames3D[5],center);
        } else {
            ImGuiID leftBottom=ImGui::DockBuilderSplitNode(left,ImGuiDir_Down,.5f,nullptr,&left);
            ImGuiID rightBottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.5f,nullptr,&center);
            ImGui::DockBuilderDockWindow(panelNames3D[2],left);
            ImGui::DockBuilderDockWindow(panelNames3D[3],center);
            ImGui::DockBuilderDockWindow(panelNames3D[4],leftBottom);
            ImGui::DockBuilderDockWindow(panelNames3D[5],rightBottom);
        }
    }
    ImGui::DockBuilderFinish(dockId);
    std::fill_n(s.panelsOpen3D,7,true);s.resetLayout3D=false;
}

// Compose an isolated, persistent dockspace with a fixed toolbar and compact physical status bar.
bool renderScreen3D(Seis3D_State& s,int width,int height) {
    (void)width;(void)height;
    ImGui::PushFont(nullptr,ImGui::GetFontSize()*.86f);
    s.homeRequested3D=false;
    if(!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()) {
        if(ImGui::IsKeyPressed(ImGuiKey_Space,false)&&s.ready3D) s.running3D=!s.running3D;
        if(ImGui::IsKeyPressed(ImGuiKey_C,false)&&s.ready3D&&!s.recording3D) performAction3D(s,[&]{resetGpu3D(s.gpu3D,s.context3D);s.step3D=0;s.dirty3D=true;});
        if(ImGui::IsKeyPressed(ImGuiKey_R,false)) s.camera3D=VolumeCamera3D{};
    }
    if(ImGui::GetTime()-s.memoryRefreshTime3D>1) {
        s.memoryRefreshTime3D=ImGui::GetTime();
        if(cudaMemGetInfo(&s.freeVram3D,&s.totalVram3D)!=cudaSuccess) s.freeVram3D=s.totalVram3D=0;
    }
    performAction3D(s,[&]{advanceSimulation3D(s);});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.04f,.065f,.075f,1));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.04f,.065f,.075f,1));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.22f,.47f,.26f,.8f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,ImVec4(.48f,.92f,.25f,1));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.12f,.24f,.16f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.22f,.4f,.18f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.3f,.55f,.19f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(.43f,.76f,.3f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(.57f,.96f,.34f,1));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,ImVec4(.15f,.29f,.2f,1));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,ImVec4(.23f,.42f,.24f,1));
    ImGui::PushStyleColor(ImGuiCol_DockingPreview,ImVec4(.45f,.85f,.3f,.5f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected,ImVec4(.12f,.24f,.18f,1));
    ImGui::PushStyleColor(ImGuiCol_TabDimmed,ImVec4(.07f,.12f,.12f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.09f,.16f,.17f,1));
    ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(.12f,.24f,.18f,1));
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("Seis3D Dock Host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);
    if(ImGui::Button("Home")) s.homeRequested3D=true;
    ImGui::SameLine();ImGui::TextColored(ImVec4(.57f,.94f,.34f,1),"SEIS3D");
    ImGui::SameLine();ImGui::BeginDisabled(!s.ready3D);
    if(ImGui::Button(s.running3D?"Pause":"Run")) s.running3D=!s.running3D;
    ImGui::SameLine();if(ImGui::Button("Step")) {s.running3D=false;s.singleStep3D=true;}
    ImGui::SameLine();ImGui::BeginDisabled(s.recording3D);
    if(ImGui::Button("Reset fields")) performAction3D(s,[&]{resetGpu3D(s.gpu3D,s.context3D);s.step3D=0;s.running3D=false;s.dirty3D=true;});
    ImGui::EndDisabled();ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("Workspace")) ImGui::OpenPopup("WorkspaceMenu3D");
    if(ImGui::BeginPopup("WorkspaceMenu3D")) {
        if(ImGui::MenuItem("Four views",nullptr,s.layoutPreset3D==0)) {s.layoutPreset3D=0;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Volume focus",nullptr,s.layoutPreset3D==1)) {s.layoutPreset3D=1;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Compact / tabbed",nullptr,s.layoutPreset3D==2)) {s.layoutPreset3D=2;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Restore current preset")) s.resetLayout3D=true;
        ImGui::Separator();
        const char* labels[]={"Simulation controls","Display / materials","Inline","Crossline","Depth","Volume","Receiver traces"};
        for(int i=0;i<7;++i) ImGui::MenuItem(labels[i],nullptr,&s.panelsOpen3D[i]);
        ImGui::EndPopup();
    }
    ImGui::SameLine();ImGui::TextDisabled("%s | %.0f FPS | %.2f ms/batch",s.running3D?"RUNNING":s.ready3D?"PAUSED":"SETUP",ImGui::GetIO().Framerate,s.computeMs3D);
    ImGui::Text("t %.4f s | step %llu | Inline Y %d / Crossline X %d | VRAM %.2f / %.2f GiB",
        s.step3D*double(s.context3D.dt3D),static_cast<unsigned long long>(s.step3D),s.slice3D[1],s.slice3D[0],
        (s.totalVram3D-s.freeVram3D)/gibibyte3D,s.totalVram3D/gibibyte3D);
    ImGuiWindowClass windowClass;windowClass.ClassId=ImGui::GetID("Seis3D.IsolatedWindows");windowClass.DockingAllowUnclassed=false;
    const ImGuiID dockId=ImGui::GetID("Seis3D.Dockspace.v2");
    const ImVec2 dockSize((std::max)(1.0f,ImGui::GetContentRegionAvail().x),
        (std::max)(60.0f,ImGui::GetContentRegionAvail().y-ImGui::GetTextLineHeightWithSpacing()*2-10));
    if(!ImGui::DockBuilderGetNode(dockId)||s.resetLayout3D) buildDockLayout3D(s,dockId,dockSize);
    ImGui::DockSpace(dockId,dockSize,ImGuiDockNodeFlags_None,&windowClass);
    if(s.ready3D&&s.hoverValid3D) {
        const auto q=s.hoverPoint3D;const float h=s.context3D.spacing3D;
        ImGui::Text("XYZ [%d,%d,%d]  (%.0f,%.0f,%.0f) m | Vp %.0f / Vs %.0f m/s | rho %.0f kg/m3",
            q.x3D,q.y3D,q.z3D,q.x3D*h,q.y3D*h,q.z3D*h,s.hoverMaterial3D[0],s.hoverMaterial3D[1],s.hoverMaterial3D[2]);
    } else if(s.ready3D) {
        const float* range=s.materialRangeAuto3D?s.materialRange3D:s.manualMaterialRange3D;
        ImGui::TextDisabled("%s %.0f .. %.0f %s | Wave max %.2e | Hover: probe / drag tabs: dock",
            s.materialComponent3D==5?"Vp":s.materialComponent3D==6?"Vs":"Density",range[0],range[1],
            s.materialComponent3D==7?"kg/m3":"m/s",s.viewMaximum3D);
    } else ImGui::TextDisabled("Initialize a model in Simulation | Workspace: layouts and panels | Space: run/pause");
    ImGui::TextColored(s.messageIsError3D?ImVec4(1,.55f,.38f,1):ImVec4(.6f,.78f,.65f,1),"%s",s.message3D.c_str());
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",s.message3D.c_str());
    ImGui::End();
    for(int panel=0;panel<7;++panel) {
        // Apply controls before rendering, and render textures before submitting ImGui images.
        if(panel==2) performAction3D(s,[&]{updateGraphics3D(s);});
        if(!s.panelsOpen3D[panel]) continue;
        ImGui::SetNextWindowClass(&windowClass);
        ImGui::SetNextWindowSize(ImVec2(400,320),ImGuiCond_FirstUseEver);
        if(ImGui::Begin(panelNames3D[panel],&s.panelsOpen3D[panel])) {
            ImGui::PushItemWidth((std::max)(80.0f,ImGui::GetContentRegionAvail().x*.55f));
            if(panel==0 && ImGui::BeginTabBar("ControlTabs3D",ImGuiTabBarFlags_FittingPolicyScroll)) {
                if(ImGui::BeginTabItem("Simulation")) {drawSimulationControls3D(s);ImGui::EndTabItem();}
                if(ImGui::BeginTabItem("Source")) {drawSourceControls3D(s);ImGui::EndTabItem();}
                if(ImGui::BeginTabItem("Model I/O")) {drawModelControls3D(s);ImGui::EndTabItem();}
                if(ImGui::BeginTabItem("Acquisition")) {drawAcquisitionControls3D(s);ImGui::EndTabItem();}
                ImGui::EndTabBar();
            }
            if(panel==1) {drawViewControls3D(s);drawLegend3D(s);}
            if(panel>=2&&panel<=4) drawSlicePanel3D(s,panel-2);
            if(panel==5) drawVolumePanel3D(s);
            if(panel==6) {
                if(s.gather3D.sampleCount3D==0) ImGui::TextWrapped("Deploy receivers and capture a shot in Acquisition to inspect Vx / Vy / Vz traces here.");
                drawTracePreview3D(s);
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();
    }
    ImGui::PopStyleColor(16);ImGui::PopFont();
    return s.homeRequested3D;
}

} // namespace Seis3D
