#include "TraceProcessing.h"
#include "AppWidgets.h"
#include "Internal3D.h"
#include "AppTheme.h"
#include "imgui.h"
#include "implot.h"
#include "RecordVisuals.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <numeric>
#include <cstdio>
#include <utility>

namespace Seis3D::Detail3D {
// Group by the fixed horizontal coordinate and depth, keeping stored receiver IDs intact.
ReceiverSelection3D selectReceiverRow3D(const std::vector<Receiver3D>& receivers,int mode,int row,int selected) {
    ReceiverSelection3D result;
    if(receivers.empty()) return result;
    if(mode==0) {
        result.indices3D.push_back((std::clamp)(selected,0,int(receivers.size())-1));return result;
    }
    if(mode==1) {
        result.indices3D.resize(receivers.size());std::iota(result.indices3D.begin(),result.indices3D.end(),0);return result;
    }
    const auto key=[&](const Receiver3D& p){return std::pair<int,int>(mode==2?p.y3D:p.x3D,p.z3D);};
    std::vector<std::pair<int,int>> rows;
    for(const auto& p:receivers) rows.push_back(key(p));
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    result.rowCount3D=int(rows.size());result.row3D=(std::clamp)(row,0,result.rowCount3D-1);
    for(int i=0;i<int(receivers.size());++i) if(key(receivers[i])==rows[result.row3D]) result.indices3D.push_back(i);
    std::stable_sort(result.indices3D.begin(),result.indices3D.end(),[&](int a,int b){
        return mode==2?receivers[a].x3D<receivers[b].x3D:receivers[a].y3D<receivers[b].y3D;
    });
    result.anchor3D=receivers[result.indices3D.front()];return result;
}

// Equalize quiet arrivals with a centered time window, a smooth noise gate and a hard boost cap.
void compensateTraceAgc3D(std::vector<double>& values,const std::vector<double>& times,const TraceAgcSettings3D& settings) {
    Seismic::applyAgc(values,times,{settings.enabled3D,settings.windowMs3D,settings.strength3D,settings.maxBoost3D,settings.noiseFloorPercent3D});
}

// Bound plotting work to 2048 points after AGC so narrow compensated arrivals survive reduction.
TraceSeries3D buildTraceSeries3D(const Seis3D_State& s,bool captured,int receiver,int component,bool imported) {
    TraceSeries3D result;
    const auto& gather=imported?s.importedShot3D.gather3D:s.gather3D;
    const int receiverCount=captured?int(gather.receivers3D.size()):s.liveTrace3D.receiverCount3D;
    const int count=captured?gather.sampleCount3D:s.liveTrace3D.count3D;
    if(receiver<0||receiver>=receiverCount||component<0||component>3||count<1) return result;
    if(imported&&component!=s.importedShot3D.component3D) return result;
    const auto& data=captured?gather.samples3D:s.liveTrace3D.samples3D;
    const int storedRows=captured?count:LiveTrace3D::capacity3D;
    if((!captured&&count>storedRows)||data.size()<std::size_t(storedRows)*receiverCount*3)
        throw std::runtime_error("Receiver preview sample buffer does not match its geometry.");
    const bool cacheable=captured&&(imported||!s.recording3D);
    auto& cache=s.tracePreviewCache3D;
    const auto& settings=s.traceAgc3D;
    if(cacheable) {
        const bool changed=cache.samples3D!=data.data()||cache.valueCount3D!=data.size()||cache.sampleCount3D!=count
            ||cache.receiverCount3D!=receiverCount||cache.sampleIntervalUs3D!=gather.sampleIntervalUs3D
            ||cache.firstSampleTime3D!=gather.firstSampleTime3D||cache.imported3D!=imported
            ||cache.settings3D.enabled3D!=settings.enabled3D||cache.settings3D.windowMs3D!=settings.windowMs3D
            ||cache.settings3D.strength3D!=settings.strength3D||cache.settings3D.maxBoost3D!=settings.maxBoost3D
            ||cache.settings3D.noiseFloorPercent3D!=settings.noiseFloorPercent3D;
        if(changed) {
            cache.entries3D.clear();cache.densityReceivers3D.clear();cache.samples3D=data.data();cache.valueCount3D=data.size();
            cache.sampleCount3D=count;cache.receiverCount3D=receiverCount;cache.sampleIntervalUs3D=gather.sampleIntervalUs3D;
            cache.firstSampleTime3D=gather.firstSampleTime3D;cache.imported3D=imported;cache.settings3D=settings;
        }
        const auto found=cache.entries3D.find(receiver*4+component);
        if(found!=cache.entries3D.end()) return found->second;
    }
    const auto row=[&](int i){return captured?i:(s.liveTrace3D.next3D-count+i+LiveTrace3D::capacity3D)%LiveTrace3D::capacity3D;};
    const auto value=[&](int i){
        const auto offset=(std::size_t(row(i))*receiverCount+receiver)*3;
        const double x=data[offset],y=data[offset+1],z=data[offset+2];
        const double v=component==3?std::sqrt(x*x+y*y+z*z):data[offset+component];
        if(!std::isfinite(v)) throw std::runtime_error("Non-finite sample in receiver preview.");
        return v;
    };
    std::vector<double> fullTimes;fullTimes.reserve(count);
    std::vector<double> fullValues;fullValues.reserve(count);
    for(int i=0;i<count;++i) {
        const double sample=value(i);
        result.rawPeak3D=std::max(result.rawPeak3D,std::abs(sample));
        fullValues.push_back(sample);
        fullTimes.push_back(captured?(imported?s.importedShot3D.firstTimes3D[receiver]:gather.firstSampleTime3D)+i*gather.sampleIntervalUs3D*1e-6:s.liveTrace3D.times3D[row(i)]);
    }
    compensateTraceAgc3D(fullValues,fullTimes,s.traceAgc3D);
    result.times3D.reserve((std::min)(count,2048));result.values3D.reserve((std::min)(count,2048));
    int last=-1;
    const auto append=[&](int i){
        if(i==last) return;
        result.times3D.push_back(fullTimes[i]);
        result.values3D.push_back(fullValues[i]);last=i;
    };
    if(count<=2048) {
        for(int i=0;i<count;++i) {result.peak3D=(std::max)(result.peak3D,std::abs(fullValues[i]));append(i);}
    } else {
        append(0);
        const int bucket=(count+1022)/1023;
        for(int begin=0;begin<count;begin+=bucket) {
            int low=begin,high=begin;double minimum=fullValues[begin],maximum=minimum;
            for(int i=begin;i<(std::min)(begin+bucket,count);++i) {
                const double v=fullValues[i];result.peak3D=(std::max)(result.peak3D,std::abs(v));
                if(v<minimum) {minimum=v;low=i;}if(v>maximum) {maximum=v;high=i;}
            }
            append((std::min)(low,high));append((std::max)(low,high));
        }
        append(count-1);
    }
    if(cacheable) {
        // At most four components per actual receiver; larger sections must retain their cached AGC previews.
        if(cache.entries3D.size()>=std::size_t(receiverCount)*4) cache.entries3D.clear();
        cache.entries3D.emplace(receiver*4+component,result);
    }
    return result;
}

namespace {
struct WigglePlot3D {
    const TraceSeries3D* series3D=nullptr;
    double receiver3D=0, gain3D=1;
    int lobe3D=0;
};
// Place velocity excursions around a receiver baseline, with true sample time increasing downward.
ImPlotPoint wigglePoint3D(int index,void* pointer) {
    const auto& p=*static_cast<WigglePlot3D*>(pointer);
    const double v=p.series3D->values3D[index];
    return {p.receiver3D+(p.lobe3D>0?(std::max)(0.0,v):p.lobe3D<0?(std::min)(0.0,v):v)*p.gain3D,p.series3D->times3D[index]};
}
// Provide the vertical zero-amplitude curve for positive variable-area filling.
ImPlotPoint baselinePoint3D(int index,void* pointer) {
    const auto& p=*static_cast<WigglePlot3D*>(pointer);return {p.receiver3D,p.series3D->times3D[index]};
}

// Keep single and multi-channel preview controls consistent and independent from recorded samples.
void drawTraceOptions3D(Seis3D_State& s,bool& fit) {
    if(AppUi::Widgets::Button("Trace options")) ImGui::OpenPopup("TraceOptions3D");
    if(!ImGui::BeginPopup("TraceOptions3D")) return;
    ImGui::SetNextItemWidth(250);
    fit|=AppUi::Widgets::SliderFloat("Trace gain",&s.traceGain3D,.1f,20,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    fit|=AppUi::Widgets::Checkbox("Automatic gain compensation",&s.traceAgc3D.enabled3D);
    if(s.traceAgc3D.enabled3D) {
        ImGui::SetNextItemWidth(250);fit|=AppUi::Widgets::SliderFloat("AGC window (ms)",&s.traceAgc3D.windowMs3D,5,2000,"%.0f ms",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(250);fit|=AppUi::Widgets::SliderFloat("AGC strength",&s.traceAgc3D.strength3D,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(250);fit|=AppUi::Widgets::SliderFloat("Maximum AGC boost",&s.traceAgc3D.maxBoost3D,1,200,"%.0fx",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(250);fit|=AppUi::Widgets::SliderFloat("Noise floor (percent of peak)",&s.traceAgc3D.noiseFloorPercent3D,0,2,"%.3f%%",ImGuiSliderFlags_AlwaysClamp);
        if(AppUi::Widgets::Button("Reset AGC parameters")) {s.traceAgc3D=TraceAgcSettings3D{true};fit=true;}
        AppUi::Widgets::TextWrapped("AGC changes relative amplitudes in the preview. SEG-Y export keeps original samples.");
    }
    fit|=AppUi::Widgets::Checkbox("Normalize each trace",&s.traceNormalizeEach3D);
    AppUi::Widgets::TextWrapped(s.traceNormalizeEach3D?"Each channel has its own scale; relative amplitudes are not comparable.":"One amplitude scale for the selected channels; relative amplitudes are preserved.");
    if(AppUi::Widgets::Checkbox("Positive variable area",&s.tracePositiveFill3D)) {s.traceStyle3D=6;if(!s.tracePositiveFill3D) s.traceSignedFill3D=false;}
    fit|=AppUi::Widgets::Checkbox("Auto fit traces",&s.traceAutoFit3D);
    if(AppUi::Widgets::Button("Fit traces now")) fit=true;
    if(AppUi::Widgets::Button("Reset viewer display")) {
        s.traceGain3D=s.traceHeatmapRange3D=1;s.traceDensityOpacity3D=.85f;s.tracePalette3D=1;s.traceStyle3D=3;
        s.traceAgc3D={};s.traceNormalizeEach3D=false;s.traceAutoFit3D=true;
        Seismic::applyRecordStyle(3,s.traceShowWiggle3D,s.traceShowHeatmap3D,s.tracePositiveFill3D,s.traceSignedFill3D);fit=true;
    }
    AppUi::Widgets::TextWrapped("Trace gain changes this preview only. Wavefield gain and recorded samples are independent.");
    ImGui::EndPopup();
}

// Expose the same record styles/palettes as the 2D viewer without changing wavefield settings.
void drawRecordAppearance3D(Seis3D_State& s) {
    if(AppUi::Widgets::Button("Display Parameters")) ImGui::OpenPopup("RecordAppearance3D");
    if(ImGui::BeginPopup("RecordAppearance3D")) {
        Seismic::drawRecordVisualControls(s.traceStyle3D,s.tracePalette3D,s.traceHeatmapRange3D,s.traceDensityOpacity3D,
            s.traceShowWiggle3D,s.traceShowHeatmap3D,s.tracePositiveFill3D,s.traceSignedFill3D);
        ImGui::EndPopup();
    }
}

// Follow actual data counts in all-trace mode, or retain a valid user-selected window after data changes.
void updateTraceWindow3D(Seis3D_State& s,int total) {
    if(total<1) {s.traceFirst3D=0;s.traceVisibleCount3D=0;return;}
    if(s.traceShowAll3D) {s.traceFirst3D=0;s.traceVisibleCount3D=total;return;}
    s.traceFirst3D=(std::clamp)(s.traceFirst3D,0,total-1);
    s.traceVisibleCount3D=(std::clamp)(s.traceVisibleCount3D,1,total-s.traceFirst3D);
}

// Draw a selected receiver arrangement using independent trace gain and explicit normalization.
void drawMultiTrace3D(Seis3D_State& s,bool captured,ReceiverSelection3D selection,bool fit,bool imported) {
    const auto& gather=imported?s.importedShot3D.gather3D:s.gather3D;
    const auto& receivers=captured?gather.receivers3D:s.receivers3D;
    const auto& unit=imported?s.importedShot3D.amplitudeUnit3D:std::string("m/s");
    const auto& p=AppUi::palette();
    if(ImGui::BeginTable("TraceRowControls3D",3,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Component",ImGuiTableColumnFlags_WidthStretch,.26f);
        ImGui::TableSetupColumn("Row",ImGuiTableColumnFlags_WidthStretch,.48f);
        ImGui::TableSetupColumn("Options",ImGuiTableColumnFlags_WidthStretch,.26f);
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
        if(imported) AppUi::Widgets::TextUnformatted(s.importedShot3D.componentName3D.c_str());
        else fit|=AppUi::Widgets::Combo("##TraceComponent3D",&s.traceComponent3D,"Vx\0Vy\0Vz\0Velocity magnitude\0");
        ImGui::TableNextColumn();
        if(s.traceDisplay3D>=2) {
            int row=selection.row3D+1;ImGui::SetNextItemWidth(-1);
            if(AppUi::Widgets::SliderInt("##ReceiverRow3D",&row,1,selection.rowCount3D,AppUi::text("Row %d"))) {
                s.traceRow3D=row-1;selection=selectReceiverRow3D(receivers,s.traceDisplay3D,s.traceRow3D);fit=true;
            }
            s.traceRow3D=selection.row3D;
        } else AppUi::Widgets::Text("%d channels",int(selection.indices3D.size()));
        ImGui::TableNextColumn();
        drawTraceOptions3D(s,fit);
        ImGui::EndTable();
    }
    const double spacing=captured?gather.context3D.spacing3D:s.context3D.spacing3D;
    const int total=int(selection.indices3D.size());
    if(total<1) return;
    updateTraceWindow3D(s,total);
    if(total>1) {
        int window[2]={s.traceFirst3D+1,s.traceVisibleCount3D};ImGui::SetNextItemWidth(220);
        if(AppUi::Widgets::InputInt2("First / visible traces",window)) {
            s.traceShowAll3D=false;fit=true;
            s.traceFirst3D=(std::clamp)(window[0],1,total)-1;s.traceVisibleCount3D=window[1];
        }
        ImGui::SameLine();if(AppUi::Widgets::SmallButton("Show all traces")) {s.traceShowAll3D=true;fit=true;}
        updateTraceWindow3D(s,total);
        AppUi::Widgets::TextDisabled("Visible %d / %d | Maximum from first trace: %d",s.traceVisibleCount3D,total,total-s.traceFirst3D);
    }
    const int end=s.traceFirst3D+s.traceVisibleCount3D;
    selection.indices3D=std::vector<int>(selection.indices3D.begin()+s.traceFirst3D,selection.indices3D.begin()+end);
    if(s.traceDisplay3D>=2) AppUi::Widgets::Text("%s row %d/%d | %s %.3f %s | %d channels",
        s.traceDisplay3D==2?"X":"Y",selection.row3D+1,selection.rowCount3D,s.traceDisplay3D==2?"Y":"X",
        imported?s.importedShot3D.coordinates3D[selection.indices3D.front()][s.traceDisplay3D==2?1:0]:
            (s.traceDisplay3D==2?selection.anchor3D.y3D:selection.anchor3D.x3D)*spacing,
        imported?s.importedShot3D.coordinateUnit3D.c_str():"m",int(selection.indices3D.size()));
    std::vector<TraceSeries3D> series;double commonPeak=0,rawPeak=0,timeMin=1e100,timeMax=-1e100;
    for(int receiver:selection.indices3D) {
        series.push_back(buildTraceSeries3D(s,captured,receiver,imported?s.importedShot3D.component3D:s.traceComponent3D,imported));
        commonPeak=(std::max)(commonPeak,series.back().peak3D);
        rawPeak=(std::max)(rawPeak,series.back().rawPeak3D);
        if(!series.back().times3D.empty()) {timeMin=(std::min)(timeMin,series.back().times3D.front());timeMax=(std::max)(timeMax,series.back().times3D.back());}
    }
    if(series.empty()||series.front().times3D.empty()) return;
    timeMax=(std::max)(timeMax,timeMin+1e-6);
    auto& densityCache=s.tracePreviewCache3D;
    if(s.traceShowHeatmap3D) {
        const bool changing=!captured||(!imported&&s.recording3D);
        if(changing||densityCache.densityReceivers3D!=selection.indices3D
            ||densityCache.densityComponent3D!=(imported?s.importedShot3D.component3D:s.traceComponent3D)
            ||densityCache.densityNormalize3D!=s.traceNormalizeEach3D||densityCache.densityMin3D!=timeMin||densityCache.densityMax3D!=timeMax) {
            int rows=1;for(const auto& trace:series) rows=(std::max)(rows,(std::min)(1024,int(trace.times3D.size())));
            densityCache.density3D=Seismic::buildTraceDensity(int(series.size()),rows,timeMin,timeMax,commonPeak,s.traceNormalizeEach3D,
                [&](int i)->const auto&{return series[i].times3D;},[&](int i)->const auto&{return series[i].values3D;},[&](int i){return series[i].peak3D;});
            densityCache.densityRows3D=rows;densityCache.densityReceivers3D=selection.indices3D;
            densityCache.densityComponent3D=imported?s.importedShot3D.component3D:s.traceComponent3D;
            densityCache.densityNormalize3D=s.traceNormalizeEach3D;densityCache.densityMin3D=timeMin;densityCache.densityMax3D=timeMax;
        }
    }
    if(s.traceAgc3D.enabled3D) AppUi::Widgets::TextDisabled("Raw peak %.2e %s | AGC display peak %.2e | Trace gain %.2fx",rawPeak,unit.c_str(),commonPeak,s.traceGain3D);
    else AppUi::Widgets::TextDisabled("%s peak %.2e %s | Trace gain %.2fx",s.traceNormalizeEach3D?"Individual scale; selected":"Shared",commonPeak,unit.c_str(),s.traceGain3D);
    if(rawPeak<1e-15&&ImGui::GetContentRegionAvail().y>140) AppUi::Widgets::TextDisabled("No arrival yet on the selected receivers.");
    const bool colorbar=s.traceShowHeatmap3D&&ImGui::GetContentRegionAvail().x>320;
    const float height=(std::max)(60.0f,ImGui::GetContentRegionAvail().y);
    const float colorbarWidth=Seismic::recordColorbarWidth();
    const ImVec2 plotSize((std::max)(100.0f,ImGui::GetContentRegionAvail().x-(colorbar?colorbarWidth+ImGui::GetStyle().ItemSpacing.x:0)),height);
    Seismic::beginRecordNavigation(s.traceAutoFit3D,fit,plotSize);
    const auto palette=Seismic::recordColormap(s.tracePalette3D);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,p.panel);ImPlot::PushStyleColor(ImPlotCol_PlotBorder,p.border);
    if(ImPlot::BeginPlot("Receiver section###TraceSection3D",plotSize,ImPlotFlags_NoTitle|ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes(AppUi::text("Receiver (recorded ID)"),AppUi::text("Time (s)"),ImPlotAxisFlags_None,ImPlotAxisFlags_Invert);
        if(fit||s.traceAutoFit3D) {
            const double margin=.5*s.traceGain3D+.1;
            ImPlot::SetupAxisLimits(ImAxis_X1,1-margin,double(series.size())+margin,ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1,timeMin,(std::max)(timeMax,timeMin+1e-6),ImPlotCond_Always);
        }
        std::vector<double> ticks;std::vector<std::string> labels;std::vector<const char*> pointers;
        const int stride=(std::max)(1,(int(series.size())+19)/20);
        for(int i=0;i<int(series.size());i+=stride) {ticks.push_back(i+1);labels.push_back("R"+std::to_string(imported?s.importedShot3D.traceIds3D[selection.indices3D[i]]:selection.indices3D[i]+1));}
        for(const auto& label:labels) pointers.push_back(label.c_str());
        ImPlot::SetupAxisTicks(ImAxis_X1,ticks.data(),int(ticks.size()),pointers.data());
        if(s.traceShowHeatmap3D&&!densityCache.density3D.empty()) {
            ImPlot::PushColormap(Seismic::recordColormap(s.tracePalette3D,s.traceDensityOpacity3D));
            ImPlot::PlotHeatmap("##RecordDensity3D",densityCache.density3D.data(),densityCache.densityRows3D,int(series.size()),
                -s.traceHeatmapRange3D,s.traceHeatmapRange3D,nullptr,{.5,timeMax},{double(series.size())+.5,timeMin});
            ImPlot::PopColormap();
        }
        if(s.traceShowWiggle3D) for(int i=0;i<int(series.size());++i) {
            const double peak=s.traceNormalizeEach3D?series[i].peak3D:commonPeak;
            WigglePlot3D plot{&series[i],double(i+1),.45*s.traceGain3D/(std::max)(peak,1e-30),1};
            char label[40];std::snprintf(label,sizeof(label),"R%d",selection.indices3D[i]+1);
            if(s.tracePositiveFill3D) {
                ImPlot::SetNextFillStyle(Seismic::recordPolarityColor(s.tracePalette3D,true),.32f);
                ImPlot::PlotShadedG(label,wigglePoint3D,&plot,baselinePoint3D,&plot,int(series[i].values3D.size()));
                if(s.traceSignedFill3D) {plot.lobe3D=-1;ImPlot::SetNextFillStyle(Seismic::recordPolarityColor(s.tracePalette3D,false),.32f);
                    const auto negative=std::string(label)+"##Negative";
                    ImPlot::PlotShadedG(negative.c_str(),wigglePoint3D,&plot,baselinePoint3D,&plot,int(series[i].values3D.size()));}
            }
            plot.lobe3D=0;ImPlot::SetNextLineStyle(p.text,1.1f);
            ImPlot::PlotLineG(label,wigglePoint3D,&plot,int(series[i].values3D.size()));
        }
        ImPlot::EndPlot();
    }
    if(colorbar) {ImGui::SameLine();ImPlot::ColormapScale(AppUi::text("Normalized amplitude"),-s.traceHeatmapRange3D,s.traceHeatmapRange3D,
        ImVec2(colorbarWidth,height),"%.2f",ImPlotColormapScaleFlags_None,palette);}
    ImPlot::PopStyleColor(2);
}
} // namespace

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
    AppUi::FunctionStyleScope colors(AppUi::FunctionRole::Acquisition);
    const auto& live=s.liveTrace3D;const auto& p=AppUi::palette();
    bool fit=s.traceFitRequest3D;s.traceFitRequest3D=false;
    const auto captureSelected=[&]{return s.traceView3D==3||s.traceView3D==2||(s.traceView3D==0&&s.gather3D.sampleCount3D>0&&(s.recording3D||!s.running3D));};
    if(ImGui::BeginTable("TraceHeader3D",3,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Source",ImGuiTableColumnFlags_WidthStretch,.20f);
        ImGui::TableSetupColumn("View",ImGuiTableColumnFlags_WidthStretch,.38f);
        ImGui::TableSetupColumn("Status",ImGuiTableColumnFlags_WidthStretch,.42f);
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
        fit|=AppUi::Widgets::Combo("##TraceSource3D",&s.traceView3D,"Auto\0Live\0Capture\0Imported\0");
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
        if(AppUi::Widgets::Combo("##TraceDisplay3D",&s.traceDisplay3D,s.traceView3D==3?"Single trace\0All traces\0X row (fixed Y)\0Y row (fixed X)\0":"Single (3C)\0All traces\0X row (fixed Y)\0Y row (fixed X)\0")) {
            s.traceShowAll3D=true;fit=true;
        }
        ImGui::TableNextColumn();ImGui::PushTextWrapPos(0);
        const auto& selected=s.traceView3D==3?s.importedShot3D.gather3D:s.gather3D;
        AppUi::Widgets::TextColored(AppUi::functionColor(AppUi::FunctionRole::Acquisition),s.traceView3D==3?"IMPORTED / %d samples":captureSelected()?"CAPTURE / %d samples":"LIVE / %d points",captureSelected()?selected.sampleCount3D:live.count3D);
        ImGui::PopTextWrapPos();ImGui::EndTable();
    }
    const bool captured=captureSelected();
    drawRecordAppearance3D(s);ImGui::SameLine();if(AppUi::Widgets::SmallButton("Reset View")) fit=true;
    ImGui::SameLine();Seismic::drawRecordNavigationHelp();
    const bool imported=s.traceView3D==3;const auto& g=imported?s.importedShot3D.gather3D:s.gather3D;
    if(imported&&g.sampleCount3D) AppUi::Widgets::TextDisabled("%s | Shot %d | %s [%s]",s.importedShot3D.fileName3D.c_str(),g.shotId3D,s.importedShot3D.componentName3D.c_str(),s.importedShot3D.amplitudeUnit3D.c_str());
    const int receiverCount=captured?int(g.receivers3D.size()):live.receiverCount3D;
    const int sampleCount=captured?g.sampleCount3D:live.count3D;
    if(!sampleCount||receiverCount<1) {
        if(imported) {drawShotImportControls3D(s);return;}
        AppUi::Widgets::TextWrapped(s.ready3D?"Run emits the source and monitors the receiver mesh. Fire + capture records an exportable gather.":"Initialize a model to deploy the default receiver mesh.");
        ImGui::BeginDisabled(!s.ready3D||s.recording3D);
        if(AppUi::Widgets::Button("Run with live preview")) {s.traceView3D=1;performAction3D(s,[&]{if(!s.running3D) toggleSimulation3D(s);});}
        ImGui::SameLine();if(AppUi::Widgets::Button("Fire + capture")) performAction3D(s,[&]{s.sequence3D=false;s.traceView3D=2;startGather3D(s,1);});
        ImGui::EndDisabled();return;
    }
    if(s.traceAgc3D.enabled3D) AppUi::Widgets::TextColored(AppUi::functionColor(AppUi::FunctionRole::Acquisition),
        "AGC ON | %.0f ms | strength %.2f | max %.0fx",s.traceAgc3D.windowMs3D,s.traceAgc3D.strength3D,s.traceAgc3D.maxBoost3D);
    if(s.traceDisplay3D!=0) {
        const auto& receivers=captured?g.receivers3D:s.receivers3D;
        drawMultiTrace3D(s,captured,selectReceiverRow3D(receivers,s.traceDisplay3D,s.traceRow3D),fit,imported);return;
    }
    s.selectedReceiver3D=(std::clamp)(s.selectedReceiver3D,0,receiverCount-1);
    ImGui::SetNextItemWidth((std::max)(100.0f,ImGui::GetContentRegionAvail().x*.42f));
    AppUi::Widgets::SliderInt("Receiver##TraceReceiver3D",&s.selectedReceiver3D,0,receiverCount-1);
    ImGui::SameLine();drawTraceOptions3D(s,fit);
    if(ImGui::IsItemHovered()) {
        if(imported) {const auto q=s.importedShot3D.coordinates3D[s.selectedReceiver3D];AppUi::Widgets::SetTooltip("Imported trace %d: XY %.6g, %.6g %s",s.importedShot3D.traceIds3D[s.selectedReceiver3D],q[0],q[1],s.importedShot3D.coordinateUnit3D.c_str());}
        else {const auto q=captured?g.receivers3D[s.selectedReceiver3D]:s.receivers3D[s.selectedReceiver3D];
            AppUi::Widgets::SetTooltip("Receiver %d: XYZ [%d,%d,%d]\n%s",s.selectedReceiver3D+1,q.x3D,q.y3D,q.z3D,
                captured?"Uniform sample interval; available for SEG-Y export.":"One sample per compute batch; preview only. Use capture for SEG-Y.");}
    }
    const auto vx=buildTraceSeries3D(s,captured,s.selectedReceiver3D,0,imported);
    const auto vy=buildTraceSeries3D(s,captured,s.selectedReceiver3D,1,imported);
    const auto vz=buildTraceSeries3D(s,captured,s.selectedReceiver3D,2,imported);
    const double peak=(std::max)({vx.peak3D,vy.peak3D,vz.peak3D});
    const double rawPeak=(std::max)({vx.rawPeak3D,vy.rawPeak3D,vz.rawPeak3D});
    if(rawPeak<1e-15&&ImGui::GetContentRegionAvail().y>130)
        AppUi::Widgets::TextDisabled("No arrival yet at this receiver; allow propagation time or select a nearer receiver.");
    const float plotHeight=(std::max)(60.0f,ImGui::GetContentRegionAvail().y);
    const ImVec2 plotSize(ImGui::GetContentRegionAvail().x,plotHeight);
    Seismic::beginRecordNavigation(s.traceAutoFit3D,fit,plotSize);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,p.panel);
    ImPlot::PushStyleColor(ImPlotCol_PlotBorder,p.border);
    if(ImPlot::BeginPlot(captured?"Captured receiver###TracePlot3D":"Live receiver###TracePlot3D",plotSize,ImPlotFlags_NoTitle)) {
        const std::string amplitude=s.traceAgc3D.enabled3D?AppUi::text("AGC display amplitude"):
            imported?std::string(AppUi::text("Amplitude"))+" ("+s.importedShot3D.amplitudeUnit3D+")":AppUi::text("Velocity (m/s)");
        ImPlot::SetupAxes(AppUi::text("Time (s)"),amplitude.c_str(),ImPlotAxisFlags_None,ImPlotAxisFlags_None);
        if(fit||s.traceAutoFit3D) {
            const double visiblePeak=std::max(peak,1e-20)*1.15/std::max(.1f,s.traceGain3D);
            double timeMin=1e100,timeMax=-1e100;
            for(const auto* trace:{&vx,&vy,&vz}) if(!trace->times3D.empty()) {
                timeMin=(std::min)(timeMin,trace->times3D.front());timeMax=(std::max)(timeMax,trace->times3D.back());
            }
            if(timeMin<timeMax) ImPlot::SetupAxisLimits(ImAxis_X1,timeMin,timeMax,ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1,-visiblePeak,visiblePeak,ImPlotCond_Always);
        }
        ImPlot::SetupAxisFormat(ImAxis_Y1,"%.2g");
        ImPlot::SetupLegend(ImPlotLocation_NorthEast,ImPlotLegendFlags_Horizontal);
        const auto& palette=Seismic::wavePalette(s.tracePalette3D);
        const ImVec4 colors[3]={{palette[0][0],palette[0][1],palette[0][2],1},
            {palette[3][0],palette[3][1],palette[3][2],1},{palette[4][0],palette[4][1],palette[4][2],1}};
        if(s.tracePositiveFill3D) {
            const TraceSeries3D* traces[]={&vx,&vy,&vz};
            for(int component=0;component<3;++component) if(!traces[component]->values3D.empty()) {
                Seismic::SingleTracePlot plot{&traces[component]->times3D,&traces[component]->values3D,1};
                const auto label="##ComponentFill"+std::to_string(component);
                ImPlot::SetNextFillStyle(s.traceSignedFill3D?Seismic::recordPolarityColor(s.tracePalette3D,true):colors[component],.25f);
                ImPlot::PlotShadedG(label.c_str(),Seismic::singleTracePoint,&plot,Seismic::singleTraceBaseline,&plot,int(traces[component]->values3D.size()));
                if(s.traceSignedFill3D) {plot.lobe=-1;ImPlot::SetNextFillStyle(Seismic::recordPolarityColor(s.tracePalette3D,false),.25f);
                    const auto negative=label+"Negative";
                    ImPlot::PlotShadedG(negative.c_str(),Seismic::singleTracePoint,&plot,Seismic::singleTraceBaseline,&plot,int(traces[component]->values3D.size()));}
            }
        }
        const double zero=0;ImPlot::SetNextLineStyle(p.border,1);ImPlot::PlotInfLines("##Zero",&zero,1,ImPlotInfLinesFlags_Horizontal);
        if(!vx.values3D.empty()) {ImPlot::SetNextLineStyle(colors[0],1.7f);ImPlot::PlotLine(imported?s.importedShot3D.componentName3D.c_str():"Vx",vx.times3D.data(),vx.values3D.data(),int(vx.values3D.size()));}
        if(!vy.values3D.empty()) {ImPlot::SetNextLineStyle(colors[1],1.7f);ImPlot::PlotLine(imported?s.importedShot3D.componentName3D.c_str():"Vy",vy.times3D.data(),vy.values3D.data(),int(vy.values3D.size()));}
        if(!vz.values3D.empty()) {ImPlot::SetNextLineStyle(colors[2],1.9f);ImPlot::PlotLine(imported?s.importedShot3D.componentName3D.c_str():"Vz",vz.times3D.data(),vz.values3D.data(),int(vz.values3D.size()));}
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);
}
} // namespace Seis3D::Detail3D
