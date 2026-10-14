#include "SeismicViewer2D.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace Seis2D {
namespace {
// Decode network-order SEG-Y fields without unaligned reads or signed-shift assumptions.
unsigned readU16(const unsigned char* p) {return (unsigned(p[0])<<8)|p[1];}
// Convert header shorts to their signed interpretation for delays and scalars.
int readI16(const unsigned char* p) {const auto value=readU16(p);return value<32768?int(value):int(value)-65536;}
// Assemble one network-order sample or integer header field.
std::uint32_t readU32(const unsigned char* p) {return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];}
// Reject a short read before decoding uninitialized or partial trace data.
void readExact(std::istream& input,void* buffer,std::size_t bytes) {
    input.read(static_cast<char*>(buffer),std::streamsize(bytes));if(!input) throw std::runtime_error("SEG-Y input is truncated.");
}
// Decode supported SEG-Y formats into finite float amplitudes.
float decodeSample(const unsigned char* p,int format) {
    double value=0;const auto bits=(format==1||format==2||format==5)?readU32(p):0;
    if(format==5) {float sample;std::memcpy(&sample,&bits,4);value=sample;}
    else if(format==1) {value=std::ldexp(double(bits&0xffffff)/16777216.0,4*(int((bits>>24)&127)-64));if(bits>>31) value=-value;}
    else if(format==2) {std::int32_t sample;std::memcpy(&sample,&bits,4);value=sample;}
    else if(format==3) value=readI16(p);
    else value=p[0]<128?int(p[0]):int(p[0])-256;
    if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()) throw std::runtime_error("SEG-Y contains invalid amplitudes.");
    return float(value);
}
}

// Bound host allocation before reading samples; accept common fixed-length rev0/rev1 records.
RecordData2D readRecordSegy2D(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open SEG-Y record.");
    const auto size=std::filesystem::file_size(path);if(size<3840) throw std::runtime_error("SEG-Y input is truncated.");
    input.seekg(3200);std::array<unsigned char,400> binary{};readExact(input,binary.data(),binary.size());
    const int format=int(readU16(binary.data()+24)),samples=int(readU16(binary.data()+20)),interval=int(readU16(binary.data()+16));
    const int extended=readI16(binary.data()+304);
    if(readU16(binary.data()+300)>256||extended<0) throw std::runtime_error("Unsupported SEG-Y revision or extended header count.");
    const int bytes=format==3?2:format==8?1:4;
    if(format!=1&&format!=2&&format!=3&&format!=5&&format!=8) throw std::runtime_error("Unsupported SEG-Y sample format.");
    if(samples<1||samples>32767||interval<1) throw std::runtime_error("Invalid SEG-Y sample count or interval.");
    const std::uint64_t offset=3600+std::uint64_t(extended)*3200,stride=240+std::uint64_t(samples)*bytes;
    if(offset>=size||(size-offset)%stride) throw std::runtime_error("SEG-Y trace lengths do not match the file size.");
    const auto channels=(size-offset)/stride;
    if(channels<1||channels>8192||channels*std::uint64_t(samples)*sizeof(float)>512ull*1024*1024)
        throw std::runtime_error("SEG-Y record exceeds the 8192-trace or 512 MiB viewer budget.");
    RecordData2D result;result.sampleInterval=interval*1e-6;
    const auto filename=path.filename().u8string();result.name.assign(reinterpret_cast<const char*>(filename.data()),filename.size());
    result.traces.resize(std::size_t(channels),std::vector<float>(samples));result.firstTimes.resize(std::size_t(channels));
    input.seekg(std::streamoff(offset));std::vector<unsigned char> payload(std::size_t(samples)*bytes);bool velocity=true;
    for(std::size_t channel=0;channel<channels;++channel) {
        std::array<unsigned char,240> header{};readExact(input,header.data(),header.size());readExact(input,payload.data(),payload.size());
        const int traceSamples=int(readU16(header.data()+114)),traceInterval=int(readU16(header.data()+116));
        if((traceSamples&&traceSamples!=samples)||(traceInterval&&traceInterval!=interval))
            throw std::runtime_error("Variable SEG-Y trace lengths or intervals are not supported.");
        const int scalar=readI16(header.data()+214);const double factor=scalar==0?1:scalar>0?scalar:1.0/-scalar;
        result.firstTimes[channel]=readI16(header.data()+108)*factor*.001;
        velocity=velocity&&readU16(header.data()+202)==6;
        for(int i=0;i<samples;++i) result.traces[channel][i]=decodeSample(payload.data()+std::size_t(i)*bytes,format);
    }
    if(velocity) result.units="m/s";
    return result;
}

// Commit fully parsed samples and timing while preserving gain, AGC, palettes and plot appearance.
bool importRecord2D(ViewerState2D& state,const std::filesystem::path& path) {
    try {
        auto candidate=readRecordSegy2D(path);state.records[0]=std::move(candidate);state.records[1]=RecordData2D{};
        state.dualComponent=false;state.component=0;++state.revision;state.firstTrace=state.selectedTrace=0;state.showAllTraces=true;
        state.cache=ViewerCache2D{};state.fitRequest=true;state.error=false;state.status="SEG-Y record loaded. Display settings retained.";return true;
    } catch(const std::exception& error) {state.error=true;state.status=error.what();return false;}
}

// Restore only presentation settings, leaving original data and import paths available.
void resetViewerDisplay2D(ViewerState2D& state) {
    state.showWiggle=state.showHeatmap=state.positiveFill=true;state.normalizeEach=state.autoFit=false;
    state.gain=state.heatmapRange=1;state.agc=Seismic::AgcSettings{};state.palette=1;state.recordStyle=3;
    state.signedFill=false;state.densityOpacity=.85f;state.fitRequest=true;
}

// Process one complete trace at a time; visible channels follow actual data and density has at most 1024 time rows.
void prepareViewerCache2D(ViewerState2D& state) {
    const auto& record=state.records[state.dualComponent?std::clamp(state.component,0,1):0];
    if(record.traces.empty()) {state.cache=ViewerCache2D{};return;}
    state.firstTrace=std::clamp(state.firstTrace,0,int(record.traces.size())-1);
    if(state.showAllTraces&&state.viewMode==0) {state.firstTrace=0;state.visibleTraces=int(record.traces.size());}
    state.visibleTraces=std::clamp(state.visibleTraces,1,int(record.traces.size())-state.firstTrace);
    state.selectedTrace=std::clamp(state.selectedTrace,0,int(record.traces.size())-1);
    const int first=state.viewMode==1?state.selectedTrace:state.firstTrace;
    const int count=state.viewMode==1?1:std::min(state.visibleTraces,int(record.traces.size())-first);
    auto& cache=state.cache;
    if(cache.revision==state.revision&&cache.first==first&&cache.count==count&&cache.component==state.component
        &&cache.agc==state.agc&&cache.normalize==state.normalizeEach) return;
    ViewerCache2D next;next.revision=state.revision;next.first=first;next.count=count;next.component=state.component;
    next.agc=state.agc;next.normalize=state.normalizeEach;next.timeMin=std::numeric_limits<double>::max();next.timeMax=-next.timeMin;
    const int samples=int(record.traces.front().size());next.heatmapRows=std::min(samples,1024);
    if(samples<1||record.firstTimes.size()!=record.traces.size()||!std::isfinite(record.sampleInterval)||record.sampleInterval<=0)
        throw std::runtime_error("Invalid record geometry or timing.");
    for(int channel=0;channel<count;++channel) {
        const int index=first+channel;const auto& raw=record.traces[index];
        if(int(raw.size())!=samples) throw std::runtime_error("Record traces have different lengths.");
        std::vector<double> times(samples),processed(raw.begin(),raw.end());
        for(int i=0;i<samples;++i) {times[i]=record.firstTimes[index]+i*record.sampleInterval;next.rawPeak=std::max(next.rawPeak,std::abs(double(raw[i])));}
        Seismic::applyAgc(processed,times,state.agc);
        next.previews.push_back(Seismic::reduceExtrema(processed,times));
        next.peak=std::max(next.peak,next.previews.back().peak);next.timeMin=std::min(next.timeMin,times.front());next.timeMax=std::max(next.timeMax,times.back());
    }
    if(next.timeMax<=next.timeMin) next.timeMax=next.timeMin+record.sampleInterval;
    next.heatmap=Seismic::buildTraceDensity(count,next.heatmapRows,next.timeMin,next.timeMax,next.peak,state.normalizeEach,
        [&](int i)->const auto&{return next.previews[i].times;},[&](int i)->const auto&{return next.previews[i].values;},
        [&](int i){return next.previews[i].peak;});
    cache=std::move(next);
}
}
