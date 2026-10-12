#include "Internal3D.h"
#include "Visual3D.h"
#include "imgui_internal.h"
#include "AppTheme.h"
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
        ImGui::TextColored(budget.totalBytes3D<=memoryLimit3D?AppUi::palette().accent:ImVec4(1,0.45f,0.3f,1),
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
        restartSimulation3D(s,true);
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

// Restore a complete, readable display recipe without touching physical amplitudes or acquisition.
void applyVisualPreset3D(Seis3D_State& s,int preset) {
    s.visualPreset3D=(std::clamp)(preset,0,4);s.displayMode3D=0;s.autoScale3D=true;
    s.materialRangeAuto3D=true;s.waveOpacity3D=.95f;s.waveThreshold3D=.008f;s.waveGamma3D=.6f;
    s.materialBrightness3D=.65f;s.gain3D=1;s.modelOpacity3D=.12f;s.gridOpacity3D=.10f;
    s.volumeThreshold3D=.025f;s.volumeOpacity3D=2;s.showVolume3D=true;
    s.component3D=preset==1?0:3;s.waveStyle3D=preset==1?0:preset==2?6:preset==3?13:7;
    s.modelStyle3D=preset==2?1:0;
    if(preset==2) {s.materialBrightness3D=.9f;s.modelOpacity3D=.3f;s.waveGamma3D=.8f;}
    if(preset==4) {
        s.component3D=3;s.waveStyle3D=1;s.modelStyle3D=4;
        s.materialBrightness3D=1;s.modelOpacity3D=.16f;s.gridOpacity3D=.16f;
        s.gain3D=1.8f;s.waveOpacity3D=1;s.waveGamma3D=.5f;
        s.volumeOpacity3D=1.4f;s.volumeThreshold3D=.02f;
    }
    s.dirty3D=true;
}

// Configure independent geological material, signed wave transfer functions and rendering cadence.
void drawViewControls3D(Seis3D_State& s) {
    AppUi::drawThemeSelector(fieldLabel3D("Interface theme").c_str(),-1);
    int preset=s.visualPreset3D;
    if(ImGui::Combo(fieldLabel3D("Visual preset").c_str(),&preset,"Wavefront contrast\0Signed seismic\0Geological layers\0CUDA glow\0Paper scientific\0")) applyVisualPreset3D(s,preset);
    if(ImGui::Button("Restore readable display",ImVec2(-1,0))) applyVisualPreset3D(s,AppUi::palette().light?4:0);
    ImGui::SeparatorText("COMPOSITION");
    ImGui::Combo(fieldLabel3D("Display").c_str(),&s.displayMode3D,"Model + wavefield\0Wavefield only\0Material only\0");
    if(ImGui::Combo(fieldLabel3D("Wave component").c_str(),&s.component3D,"Vz\0Vx\0Vy\0Velocity magnitude\0Pressure\0")) s.dirty3D=true;
    if(s.component3D<3) ImGui::TextWrapped("A signed component can be near zero on a symmetry plane. Use velocity magnitude to inspect all wavefronts.");
    ImGui::Combo(fieldLabel3D("Wave palette").c_str(),&s.waveStyle3D,waveStyleNames3D);
    ImGui::Checkbox("Automatic common scale",&s.autoScale3D);
    if(!s.autoScale3D) ImGui::SliderFloat(fieldLabel3D("Amplitude scale").c_str(),&s.manualScale3D,1e-8f,1e10f,"%.3e",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Display gain").c_str(),&s.gain3D,0.1f,20.0f,"%.2f",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Wave opacity").c_str(),&s.waveOpacity3D,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Wave cutoff").c_str(),&s.waveThreshold3D,0,0.8f,"%.3f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat(fieldLabel3D("Weak-wave visibility").c_str(),&s.waveGamma3D,.35f,1.5f,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Lower values reveal weak waves; samples retain SI amplitudes.");
    ImGui::PopTextWrapPos();
    ImGui::SeparatorText("GEOLOGICAL MATERIAL");
    int material=s.materialComponent3D-5;
    if(ImGui::Combo(fieldLabel3D("Property").c_str(),&material,"Vp (m/s)\0Vs (m/s)\0Density (kg/m3)\0")) {s.materialComponent3D=material+5;s.dirty3D=true;}
    ImGui::Combo(fieldLabel3D("Model palette").c_str(),&s.modelStyle3D,"Dark Titanium\0Geological Map\0Grayscale Velocity\0WaveStyle Synced\0Scientific White\0");
    ImGui::SliderFloat(fieldLabel3D("Model backdrop brightness").c_str(),&s.materialBrightness3D,.2f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
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
    if(ImGui::Button("Reset all slice views",ImVec2(-1,0))) resetSliceViews3D(s);
    if(ImGui::Button("Reset orbit camera",ImVec2(-1,0))) s.camera3D=VolumeCamera3D{};
    ImGui::TextWrapped("Slices: wheel zooms at cursor; middle drag pans; left click links positions. Each Reset view fits its slice. Volume: left drag orbits, right drag / wheel zooms, middle drag pans. R resets all views.");
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

// Configure surface receiver bins and multi-line shots, locking geometry for each active capture.
void drawAcquisitionControls3D(Seis3D_State& s) {
    auto& q=s.survey3D;
    ImGui::BeginDisabled(!s.ready3D||s.recording3D);
    ImGui::SeparatorText("SURFACE RECEIVER MESH");
    ImGui::Text("Receiver channels: %d / %d",q.receiverCountX3D*q.receiverCountY3D,maxReceivers3D);
    ImGui::TextDisabled("Total channels = X x Y; each stores Vx / Vy / Vz.");
    if(ImGui::Button("8 x 8 / 64")) {q.receiverCountX3D=q.receiverCountY3D=8;fitSurvey3D(s);}
    ImGui::SameLine();if(ImGui::Button("10 x 10 / 100")) {q.receiverCountX3D=q.receiverCountY3D=10;fitSurvey3D(s);}
    if(ImGui::InputInt(fieldLabel3D("Receiver count X").c_str(),&q.receiverCountX3D)) {
        q.receiverCountX3D=(std::clamp)(q.receiverCountX3D,1,maxReceivers3D);
        q.receiverCountY3D=(std::clamp)(q.receiverCountY3D,1,maxReceivers3D/q.receiverCountX3D);
    }
    if(ImGui::InputInt(fieldLabel3D("Receiver count Y").c_str(),&q.receiverCountY3D))
        q.receiverCountY3D=(std::clamp)(q.receiverCountY3D,1,maxReceivers3D/(std::max)(1,q.receiverCountX3D));
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

// Draw the bounded raymarching exploration view and own all orbit-camera gestures locally.
void drawVolumePanel3D(Seis3D_State& s) {
    ImGui::BeginChild("volumePanel3D",ImVec2(0,0),ImGuiChildFlags_None);
    if(!s.ready3D||!s.showVolume3D) {
        ImGui::TextWrapped("Enable the auxiliary volume in Slice View. The three slices always use full-resolution solver data.");
    } else {
        if(ImGui::SmallButton("Reset camera")) s.camera3D=VolumeCamera3D{};
        ImGui::SameLine();ImGui::TextDisabled("LMB orbit / RMB zoom / MMB pan");
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
        ImGuiID traces=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.27f,nullptr,&center);
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
        if(ImGui::IsKeyPressed(ImGuiKey_Space,false)&&s.ready3D) performAction3D(s,[&]{toggleSimulation3D(s);});
        if(ImGui::IsKeyPressed(ImGuiKey_C,false)&&s.ready3D&&!s.recording3D) performAction3D(s,[&]{restartSimulation3D(s,false);});
        if(ImGui::IsKeyPressed(ImGuiKey_R,false)) {resetSliceViews3D(s);s.camera3D=VolumeCamera3D{};}
    }
    if(ImGui::GetTime()-s.memoryRefreshTime3D>1) {
        s.memoryRefreshTime3D=ImGui::GetTime();
        if(cudaMemGetInfo(&s.freeVram3D,&s.totalVram3D)!=cudaSuccess) s.freeVram3D=s.totalVram3D=0;
    }
    performAction3D(s,[&]{advanceSimulation3D(s);});
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("Seis3D Dock Host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);
    if(ImGui::Button("Home")) s.homeRequested3D=true;
    ImGui::SameLine();ImGui::TextColored(AppUi::palette().accent,"SEIS3D");
    if(!s.ready3D) {ImGui::SameLine();if(ImGui::Button("Initialize 3D")) performAction3D(s,[&]{initializeSession3D(s,false);});}
    ImGui::SameLine();ImGui::BeginDisabled(!s.ready3D);
    const bool completed=!s.recording3D&&s.step3D>=std::uint64_t((std::clamp)(s.maximumSteps3D,1,1000000));
    if(ImGui::Button(s.running3D?"Pause":completed?"Replay":"Run")) performAction3D(s,[&]{toggleSimulation3D(s);});
    ImGui::SameLine();if(ImGui::Button("Step")) {s.running3D=false;s.singleStep3D=true;}
    ImGui::SameLine();ImGui::BeginDisabled(s.recording3D);
    if(ImGui::Button("Reset fields")) performAction3D(s,[&]{restartSimulation3D(s,false);});
    ImGui::SameLine();if(ImGui::Button("Fire + capture")) performAction3D(s,[&]{s.sequence3D=false;s.traceView3D=2;startGather3D(s,1);});
    ImGui::EndDisabled();ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("Workspace")) ImGui::OpenPopup("WorkspaceMenu3D");
    if(ImGui::BeginPopup("WorkspaceMenu3D")) {
        if(ImGui::MenuItem("Four views",nullptr,s.layoutPreset3D==0)) {s.layoutPreset3D=0;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Volume focus",nullptr,s.layoutPreset3D==1)) {s.layoutPreset3D=1;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Compact / tabbed",nullptr,s.layoutPreset3D==2)) {s.layoutPreset3D=2;s.resetLayout3D=true;}
        if(ImGui::MenuItem("Restore current preset")) s.resetLayout3D=true;
        if(ImGui::BeginMenu("Visual preset")) {
            const char* presets[]={"Wavefront contrast","Signed seismic","Geological layers","CUDA glow","Paper scientific"};
            for(int i=0;i<5;++i) if(ImGui::MenuItem(presets[i],nullptr,s.visualPreset3D==i)) applyVisualPreset3D(s,i);
            ImGui::EndMenu();
        }
        AppUi::drawThemeMenu();
        ImGui::Separator();
        const char* labels[]={"Simulation controls","Display / materials","Inline","Crossline","Depth","Volume","Receiver traces"};
        for(int i=0;i<7;++i) ImGui::MenuItem(labels[i],nullptr,&s.panelsOpen3D[i]);
        ImGui::EndPopup();
    }
    ImGui::SameLine();if(ImGui::Button("Reset views")) {resetSliceViews3D(s);s.camera3D=VolumeCamera3D{};}
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Fit all three slices and reset the orbit camera (R). Keeps wavefield and slice positions.");
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
    ImGui::TextColored(s.messageIsError3D?ImVec4(1,.55f,.38f,1):AppUi::palette().muted,"%s",s.message3D.c_str());
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
            if(panel==6) drawTracePreview3D(s);
            ImGui::PopItemWidth();
        }
        ImGui::End();
    }
    ImGui::PopFont();
    return s.homeRequested3D;
}

} // namespace Seis3D
