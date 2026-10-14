#include "AppWidgets.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif
#include "Internal3D.h"
#include "ResourcePaths.h"
#include "AppTheme.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace Seis3D::Detail3D {
namespace {
// Decode SEG-Y network-order integers without unaligned loads or aliasing.
unsigned readU16Shot3D(const unsigned char* p) {return (unsigned(p[0])<<8)|p[1];}
// Interpret the network-order short as a signed scalar or delay.
int readI16Shot3D(const unsigned char* p) {const unsigned v=readU16Shot3D(p);return v<32768?int(v):int(v)-65536;}
// Decode a full network-order word before interpreting its sample representation.
std::uint32_t readU32Shot3D(const unsigned char* p) {return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];}
// Preserve the exact two's-complement bit pattern for signed trace fields.
int readI32Shot3D(const unsigned char* p) {const auto bits=readU32Shot3D(p);std::int32_t value;std::memcpy(&value,&bits,4);return value;}
// Apply the SEG-Y signed coordinate/time scalar, including the zero-as-one convention.
double scalarShot3D(int scalar) {return scalar<0?1.0/-double(scalar):scalar>0?double(scalar):1.0;}
// Require complete headers/payloads; a partial file must never replace a valid imported shot.
void readExactShot3D(std::ifstream& input,void* output,std::size_t bytes) {
    if(!input.read(static_cast<char*>(output),std::streamsize(bytes))) throw std::runtime_error("Truncated SEG-Y header or trace samples.");
}
// Decode supported rev0/rev1 samples, rejecting overflow and non-finite data.
float sampleShot3D(const unsigned char* p,int format) {
    double value=0;
    if(format==5) {const auto bits=readU32Shot3D(p);float v;std::memcpy(&v,&bits,4);value=v;}
    else if(format==1) {
        const auto bits=readU32Shot3D(p);
        value=std::ldexp(double(bits&0xffffff)/16777216.0,4*(int((bits>>24)&127)-64));
        if(bits>>31) value=-value;
    } else if(format==2) value=readI32Shot3D(p);
    else if(format==3) value=readI16Shot3D(p);
    else value=p[0]<128?int(p[0]):int(p[0])-256;
    if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()) throw std::runtime_error("SEG-Y contains non-finite or overflowing samples.");
    return float(value);
}
#ifdef _WIN32
// Let Windows select a Unicode path without changing the application's resource working directory.
void browseShotFile3D(Seis3D_State& s) {
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"SEG-Y records\0*.sgy;*.segy\0All files\0*.*\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog)) {
        const auto path=pathToUtf8(std::filesystem::path(file));
        if(path.size()>=sizeof(s.shotImportPath3D)) {s.message3D="Selected file path exceeds the import field length.";s.messageIsError3D=true;return;}
        std::memcpy(s.shotImportPath3D,path.c_str(),path.size()+1);
    } else if(CommDlgExtendedError()) {s.message3D="Windows could not open the SEG-Y file selector.";s.messageIsError3D=true;}
}
#endif
} // namespace

// Load one fixed-length shot; exact coordinates and per-trace delay remain independent of GPU geometry.
ImportedShot3D readShotSegy3D(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open shot SEG-Y file.");
    const auto size=std::filesystem::file_size(path);if(size<3840) throw std::runtime_error("SEG-Y is too small to contain a trace.");
    input.seekg(3200);std::array<unsigned char,400> binary{};readExactShot3D(input,binary.data(),binary.size());
    const int revision=int(readU16Shot3D(binary.data()+300));
    if(revision!=0&&revision!=256) throw std::runtime_error("Supported SEG-Y revisions: big-endian rev0 and rev1.");
    const int format=int(readU16Shot3D(binary.data()+24));
    if(format!=1&&format!=2&&format!=3&&format!=5&&format!=8) throw std::runtime_error("Unsupported SEG-Y sample format. Use IBM/IEEE float or 32/16/8-bit integer.");
    const int bytes=format==3?2:format==8?1:4;
    const int extended=revision?readI16Shot3D(binary.data()+304):0;
    if(extended<0) throw std::runtime_error("Variable-count extended text headers are not supported.");
    const std::uint64_t begin=3600+std::uint64_t(extended)*3200;
    if(begin+240>size) throw std::runtime_error("SEG-Y extended headers exceed file length.");
    input.seekg(std::streamoff(begin));std::array<unsigned char,240> first{};readExactShot3D(input,first.data(),first.size());
    const int samples=readU16Shot3D(first.data()+114)?int(readU16Shot3D(first.data()+114)):int(readU16Shot3D(binary.data()+20));
    const int interval=readU16Shot3D(first.data()+116)?int(readU16Shot3D(first.data()+116)):int(readU16Shot3D(binary.data()+16));
    if(samples<1||samples>32767||interval<1) throw std::runtime_error("SEG-Y requires 1..32767 samples per trace and a positive sample interval.");
    const std::uint64_t record=240+std::uint64_t(samples)*bytes,payload=size-begin;
    if(payload%record) throw std::runtime_error("SEG-Y trace lengths are inconsistent or the file is truncated.");
    const auto traces=payload/record;
    if(traces<1||traces>4096||traces*std::uint64_t(samples)*3*sizeof(float)>256ull*1024*1024)
        throw std::runtime_error("Imported shot exceeds 4096 traces or the 256 MiB CPU sample budget.");
    ImportedShot3D shot;auto& gather=shot.gather3D;
    gather.sampleCount3D=gather.targetSamples3D=samples;gather.sampleIntervalUs3D=interval;
    gather.shotId3D=readI32Shot3D(first.data()+8);
    const int identification=int(readU16Shot3D(first.data()+28));
    shot.component3D=identification==14?0:identification==13?1:2;
    shot.componentName3D=identification==14?"Vx":identification==13?"Vy":identification==12?"Vz":"Amplitude";
    const int measurement=revision?readI16Shot3D(first.data()+202):0;
    const char* units[]={"file units","Pa","V","mV","A","m","m/s","m/s^2","N","W"};
    shot.amplitudeUnit3D=measurement>=1&&measurement<=9?units[measurement]:units[0];
    const int coordinateUnits=int(readU16Shot3D(first.data()+88));
    shot.cartesian3D=coordinateUnits==1;
    shot.coordinateUnit3D=coordinateUnits==1?"m":coordinateUnits==2?"arcsec":coordinateUnits==3?"deg":coordinateUnits==4?"DMS":"file coord";
    const int lengthUnits=int(readU16Shot3D(binary.data()+54));
    const double lengthFactor=lengthUnits==2?.3048:1.0;
    if(coordinateUnits==1&&lengthUnits!=1&&lengthUnits!=2) shot.coordinateUnit3D="file length";
    gather.samples3D.resize(std::size_t(traces)*samples*3);
    gather.receivers3D.resize(std::size_t(traces));shot.coordinates3D.resize(std::size_t(traces));
    shot.firstTimes3D.resize(std::size_t(traces));shot.traceIds3D.resize(std::size_t(traces));
    std::vector<unsigned char> data(std::size_t(samples)*bytes);std::array<double,2> source{};
    input.seekg(std::streamoff(begin));
    for(std::size_t r=0;r<traces;++r) {
        std::array<unsigned char,240> trace{};readExactShot3D(input,trace.data(),trace.size());
        const int ns=readU16Shot3D(trace.data()+114)?int(readU16Shot3D(trace.data()+114)):samples;
        const int dt=readU16Shot3D(trace.data()+116)?int(readU16Shot3D(trace.data()+116)):interval;
        if(ns!=samples||dt!=interval) throw std::runtime_error("Variable trace lengths or sample intervals are not supported.");
        if(readI32Shot3D(trace.data()+8)!=gather.shotId3D) throw std::runtime_error("File contains multiple shots. Import one shot at a time.");
        if(readI32Shot3D(trace.data()+16)!=readI32Shot3D(first.data()+16)) throw std::runtime_error("Energy source identifiers differ; expected one shot.");
        if(readU16Shot3D(trace.data()+28)!=unsigned(identification)||(revision?readI16Shot3D(trace.data()+202):0)!=measurement)
            throw std::runtime_error("Mixed trace components/units are not supported in a single imported file.");
        if(readU16Shot3D(trace.data()+88)!=unsigned(coordinateUnits)) throw std::runtime_error("Mixed coordinate units in SEG-Y traces.");
        const double coordinateScale=scalarShot3D(readI16Shot3D(trace.data()+70))*(coordinateUnits==1?lengthFactor:1.0);
        const double elevationScale=scalarShot3D(readI16Shot3D(trace.data()+68))*lengthFactor;
        const std::array<double,2> sourceHere{readI32Shot3D(trace.data()+72)*coordinateScale,readI32Shot3D(trace.data()+76)*coordinateScale};
        if(r&&sourceHere!=source) throw std::runtime_error("Source coordinates differ between traces; expected one common shot.");
        source=sourceHere;
        shot.coordinates3D[r]={readI32Shot3D(trace.data()+80)*coordinateScale,readI32Shot3D(trace.data()+84)*coordinateScale,-readI32Shot3D(trace.data()+40)*elevationScale};
        shot.firstTimes3D[r]=readI16Shot3D(trace.data()+108)*.001*(revision?scalarShot3D(readI16Shot3D(trace.data()+214)):1.0);
        const int id=readI32Shot3D(trace.data()+12);shot.traceIds3D[r]=id>0?id:int(r+1);
        readExactShot3D(input,data.data(),data.size());
        for(int i=0;i<samples;++i) gather.samples3D[(std::size_t(i)*traces+r)*3+shot.component3D]=sampleShot3D(data.data()+std::size_t(i)*bytes,format);
    }
    // Integer receiver keys are coordinate ranks; never round world metres into simulation nodes.
    for(int axis=0;axis<3;++axis) {
        std::vector<double> coordinates;for(const auto& point:shot.coordinates3D) coordinates.push_back(point[axis]);
        std::sort(coordinates.begin(),coordinates.end());coordinates.erase(std::unique(coordinates.begin(),coordinates.end()),coordinates.end());
        for(std::size_t r=0;r<traces;++r) {
            const int rank=int(std::lower_bound(coordinates.begin(),coordinates.end(),shot.coordinates3D[r][axis])-coordinates.begin());
            auto& q=gather.receivers3D[r];if(axis==0) q.x3D=rank;else if(axis==1) q.y3D=rank;else q.z3D=rank;
        }
    }
    gather.firstSampleTime3D=shot.firstTimes3D.front();shot.fileName3D=pathToUtf8(path.filename());return shot;
}

// Commit only a completely parsed shot; failed imports do not pause GPU work or destroy prior records.
bool importShotRecord3D(Seis3D_State& s,const std::filesystem::path& path) {
    try {
        auto shot=readShotSegy3D(path);s.importedShot3D=std::move(shot);
        s.tracePreviewCache3D.entries3D.clear();
        s.traceView3D=3;s.traceDisplay3D=1;s.traceRow3D=0;s.traceFirst3D=0;s.selectedReceiver3D=0;
        s.traceShowAll3D=true;s.traceVisibleCount3D=int(s.importedShot3D.gather3D.receivers3D.size());
        s.traceComponent3D=s.importedShot3D.component3D;s.panelsOpen3D[6]=true;
        s.message3D="Shot imported: "+s.importedShot3D.fileName3D;s.messageIsError3D=false;return true;
    } catch(const std::exception& e) {s.message3D=e.what();s.messageIsError3D=true;return false;}
}

// Keep record loading available without GPU initialization, with an explicit file/format boundary.
void drawShotImportControls3D(Seis3D_State& s) {
    if(!sectionHeader3D("IMPORT SHOT RECORD")) return;
    AppUi::Widgets::TextUnformatted("SEG-Y file (.sgy / .segy)");ImGui::SetNextItemWidth(-1);
    AppUi::Widgets::InputText("##ShotImportPath3D",s.shotImportPath3D,sizeof(s.shotImportPath3D));
#ifdef _WIN32
    if(AppUi::Widgets::Button("Browse SEG-Y...",ImVec2(-1,0))) browseShotFile3D(s);
#endif
    ImGui::BeginDisabled(!s.shotImportPath3D[0]);
    if(AppUi::Widgets::Button("Import shot for display",ImVec2(-1,0))) importShotRecord3D(s,uiPath3D(s.shotImportPath3D));
    ImGui::EndDisabled();
    AppUi::Widgets::TextWrapped("Paste a file path. Big-endian SEG-Y rev0/1; fixed-length, single-component, single-shot records. Import preserves simulation and captured data.");
    if(s.importedShot3D.gather3D.sampleCount3D) AppUi::Widgets::TextWrapped("%s | %d traces x %d samples | %s [%s]",
        s.importedShot3D.fileName3D.c_str(),int(s.importedShot3D.gather3D.receivers3D.size()),s.importedShot3D.gather3D.sampleCount3D,
        s.importedShot3D.componentName3D.c_str(),s.importedShot3D.amplitudeUnit3D.c_str());
}
} // namespace Seis3D::Detail3D
