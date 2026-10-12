#include "Internal3D.h"
#include "AppTheme.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace Seis3D::Detail3D {
// Retain recent receiver values for Run/Step, without inventing or exporting regular sample times.
void appendLiveTrace3D(Seis3D_State& s) {
    auto& monitor=s.liveTrace3D;
    if(monitor.receiverCount3D<1||monitor.receiverCount3D!=s.gpu3D.receiverCount3D) return;
    const auto* values=sampleReceivers3D(s.gpu3D,s.context3D);
    const std::size_t width=std::size_t(monitor.receiverCount3D)*3;
    for(std::size_t i=0;i<width;++i)
        if(!std::isfinite(values[i])) throw std::runtime_error("Non-finite live receiver sample; simulation paused.");
    std::copy_n(values,width,monitor.samples3D.data()+std::size_t(monitor.next3D)*width);
    monitor.times3D[monitor.next3D]=(double(s.step3D)-0.5)*s.context3D.dt3D;
    monitor.next3D=(monitor.next3D+1)%LiveTrace3D::capacity3D;
    monitor.count3D=(std::min)(monitor.count3D+1,LiveTrace3D::capacity3D);
}

// Keep live batch monitoring distinct from the frozen, uniformly sampled acquisition result.
void drawTracePreview3D(Seis3D_State& s) {
    const auto& g=s.gather3D;const auto& live=s.liveTrace3D;const auto& p=AppUi::palette();
    ImGui::SetNextItemWidth(150);ImGui::Combo("##TraceSource3D",&s.traceView3D,"Auto preview\0Live monitor\0Captured gather\0");
    const bool captured=s.traceView3D==2||(s.traceView3D==0&&g.sampleCount3D>0&&(s.recording3D||!s.running3D));
    const int receiverCount=captured?int(g.receivers3D.size()):live.receiverCount3D;
    const int sampleCount=captured?g.sampleCount3D:live.count3D;
    ImGui::SameLine();ImGui::TextColored(p.accent,captured?"CAPTURE / %d samples":"LIVE / %d frames",sampleCount);
    if(!sampleCount||receiverCount<1) {
        ImGui::TextWrapped(s.ready3D?"Run emits the source and monitors the receiver mesh. Fire + capture records an exportable gather.":"Initialize a model to deploy the default receiver mesh.");
        ImGui::BeginDisabled(!s.ready3D||s.recording3D);
        if(ImGui::Button("Run with live preview")) {s.traceView3D=1;performAction3D(s,[&]{if(!s.running3D) toggleSimulation3D(s);});}
        ImGui::SameLine();if(ImGui::Button("Fire + capture")) performAction3D(s,[&]{s.sequence3D=false;s.traceView3D=2;startGather3D(s,1);});
        ImGui::EndDisabled();return;
    }
    s.selectedReceiver3D=(std::clamp)(s.selectedReceiver3D,0,receiverCount-1);
    ImGui::SetNextItemWidth((std::max)(100.0f,ImGui::GetContentRegionAvail().x*.42f));
    ImGui::SliderInt("Receiver##TraceReceiver3D",&s.selectedReceiver3D,0,receiverCount-1);
    if(ImGui::IsItemHovered()) {
        const auto q=captured?g.receivers3D[s.selectedReceiver3D]:s.receivers3D[s.selectedReceiver3D];
        ImGui::SetTooltip("Receiver %d: XYZ [%d,%d,%d]\n%s",s.selectedReceiver3D+1,q.x3D,q.y3D,q.z3D,
            captured?"Uniform sample interval; available for SEG-Y export.":"One sample per compute batch; preview only. Use capture for SEG-Y.");
    }
    std::array<double,2048> times{},vx{},vy{},vz{};
    const int decimation=(std::max)(1,(sampleCount+2047)/2048);int count=0;
    double peak=0;
    for(int i=0;i<sampleCount;i+=decimation) {
        const int row=captured?i:(live.next3D-live.count3D+i+LiveTrace3D::capacity3D)%LiveTrace3D::capacity3D;
        const auto offset=(std::size_t(row)*receiverCount+s.selectedReceiver3D)*3;
        times[count]=captured?g.firstSampleTime3D+i*g.sampleIntervalUs3D*1e-6:live.times3D[row];
        const auto& data=captured?g.samples3D:live.samples3D;
        vx[count]=data[offset];vy[count]=data[offset+1];vz[count]=data[offset+2];
        peak=(std::max)({peak,std::abs(vx[count]),std::abs(vy[count]),std::abs(vz[count])});++count;
    }
    if(peak<1e-15&&ImGui::GetContentRegionAvail().y>130)
        ImGui::TextDisabled("No arrival yet at this receiver; allow propagation time or select a nearer receiver.");
    const float plotHeight=(std::max)(60.0f,ImGui::GetContentRegionAvail().y);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,p.panel);
    ImPlot::PushStyleColor(ImPlotCol_PlotBorder,p.border);
    if(ImPlot::BeginPlot(captured?"Captured receiver###TracePlot3D":"Live receiver###TracePlot3D",ImVec2(-1,plotHeight),ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxes("Time (s)","Velocity (m/s)",ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisFormat(ImAxis_Y1,"%.2g");
        ImPlot::SetupLegend(ImPlotLocation_NorthEast,ImPlotLegendFlags_Horizontal);
        const ImVec4 colors[3]={p.light?ImVec4(.05f,.39f,.65f,1):ImVec4(.2f,.75f,.97f,1),
            p.light?ImVec4(.78f,.37f,.08f,1):ImVec4(1,.65f,.22f,1),
            p.light?ImVec4(.45f,.31f,.69f,1):ImVec4(.78f,.48f,.95f,1)};
        ImPlot::SetNextFillStyle(colors[2],.12f);ImPlot::PlotShaded("##VzFill",times.data(),vz.data(),count,0.0);
        const double zero=0;ImPlot::SetNextLineStyle(p.border,1);ImPlot::PlotInfLines("##Zero",&zero,1,ImPlotInfLinesFlags_Horizontal);
        ImPlot::SetNextLineStyle(colors[0],1.7f);ImPlot::PlotLine("Vx",times.data(),vx.data(),count);
        ImPlot::SetNextLineStyle(colors[1],1.7f);ImPlot::PlotLine("Vy",times.data(),vy.data(),count);
        ImPlot::SetNextLineStyle(colors[2],1.9f);ImPlot::PlotLine("Vz",times.data(),vz.data(),count);
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);
}
} // namespace Seis3D::Detail3D
