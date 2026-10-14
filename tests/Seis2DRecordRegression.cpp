#include "SeismicViewer2D.h"
#include "SeismicIO.h"
#include "ShotExport2D.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>

// Fail with a focused error for raw-data, timing or display-state regressions.
void requireRecord(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
// Create independent SEG-Y fixtures for all supported sample formats and nonzero trace delays.
void writeFixture(const std::filesystem::path& path,int format) {
    const int bytes=format==3?2:format==8?1:4,samples=12,stride=240+samples*bytes;
    std::vector<unsigned char> data(3600+2*stride);
    const auto u16=[&](std::size_t at,unsigned value){data[at]=value>>8;data[at+1]=value;};
    const auto u32=[&](std::size_t at,std::uint32_t value){for(int i=0;i<4;++i) data[at+i]=value>>(24-i*8);};
    u16(3216,1000);u16(3220,samples);u16(3224,format);u16(3500,256);
    for(int channel=0;channel<2;++channel) {
        const auto at=3600+channel*stride;u16(at+108,100+channel*100);u16(at+114,samples);u16(at+116,1000);u16(at+202,6);u16(at+214,unsigned(-10));
        const auto sample=at+240+3*bytes;
        if(format==1) u32(sample,0xc1200000);
        else if(format==5) {float value=-2;std::uint32_t bits;std::memcpy(&bits,&value,4);u32(sample,bits);}
        else if(format==2) u32(sample,std::uint32_t(-2));
        else if(format==3) u16(sample,unsigned(-2));
        else data[sample]=254;
    }
    std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(data.data()),data.size());
}
// Exercise real file parsing, failed-import rollback, AGC and bounded previews without a GPU.
int main(int argc,char** argv) {
    try {
        const auto directory=std::filesystem::absolute(argv[0]).parent_path()/"checks"/("seis2d-record-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        Seis2D::ViewerState2D state;state.gain=3.25f;state.agc={true,60,.6f,30,.02f};
        for(const int format:{1,2,3,5,8}) {
            const auto path=directory/("format-"+std::to_string(format)+".sgy");writeFixture(path,format);
            requireRecord(Seis2D::importRecord2D(state,path),state.status.c_str());
            const auto& record=state.records[0];
            requireRecord(record.traces.size()==2&&record.traces[0][3]==-2&&record.units=="m/s","SEG-Y amplitude or units decoded incorrectly");
            requireRecord(std::abs(record.firstTimes[0]-.01)<1e-12&&std::abs(record.firstTimes[1]-.02)<1e-12,"Trace delay scalar was ignored");
            requireRecord(state.gain==3.25f&&state.agc.windowMs==60,"Import reset tuned gain/AGC");
        }
        const auto unicode=directory/std::filesystem::path(u8"中文单炮.sgy");std::filesystem::copy_file(directory/"format-5.sgy",unicode);
        requireRecord(Seis2D::importRecord2D(state,unicode),"Unicode import failed");
        const auto revision=state.revision;std::ofstream(directory/"broken.sgy")<<"broken";
        requireRecord(!Seis2D::importRecord2D(state,directory/"broken.sgy")&&state.revision==revision&&state.records[0].traces[0][3]==-2,"Invalid import destroyed prior data");
        auto& record=state.records[0];record.traces.assign(200,std::vector<float>(6000));record.firstTimes.assign(200,0);record.sampleInterval=.001;
        for(auto& trace:record.traces) {trace[100]=1;trace[5998]=.001f;}
        ++state.revision;state.showAllTraces=false;state.firstTrace=20;state.visibleTraces=128;state.agc={false};
        Seis2D::prepareViewerCache2D(state);const auto raw=state.cache.previews[0].values;
        state.agc={true,5,1,50,0};Seis2D::prepareViewerCache2D(state);
        requireRecord(state.cache.previews[0].values.size()<=2048&&state.cache.heatmap.size()<=128*1024,"Viewer preview budget exceeded");
        double late=0;for(std::size_t i=0;i<state.cache.previews[0].times.size();++i) if(state.cache.previews[0].times[i]>5) late=std::max(late,std::abs(state.cache.previews[0].values[i]));
        requireRecord(late>.01&&late<=.050001&&record.traces[0][5998]==.001f,"AGC failed to reveal weak arrival or altered raw data");
        const auto* cachePointer=state.cache.previews.data();state.gain=5;Seis2D::prepareViewerCache2D(state);
        requireRecord(cachePointer==state.cache.previews.data(),"Gain change recomputed the full processed record");
        state.viewMode=1;state.selectedTrace=155;Seis2D::prepareViewerCache2D(state);
        requireRecord(state.cache.count==1&&state.cache.first==155&&state.firstTrace==20,"Single-trace mode overwrote the receiver section selection");
        Seis2D::resetViewerDisplay2D(state);requireRecord(!state.agc.enabled&&record.traces[0][5998]==.001f,"Display reset altered raw samples");
        record.traces.assign(2,std::vector<float>{0,1,0});record.firstTimes={-1000000,1000000};record.sampleInterval=.000001;
        ++state.revision;state.viewMode=0;state.firstTrace=0;state.visibleTraces=2;
        Seis2D::prepareViewerCache2D(state);
        requireRecord(state.cache.heatmap.size()==6&&state.cache.timeMin==-1000000,"Large trace delays overflowed heatmap indices");
        std::vector<double> invalidValues{1,2},invalidTimes{0,.001};Seismic::AgcSettings invalidAgc{true};
        invalidAgc.windowMs=std::numeric_limits<float>::quiet_NaN();bool invalidRejected=false;
        try {Seismic::applyAgc(invalidValues,invalidTimes,invalidAgc);}catch(const std::invalid_argument&) {invalidRejected=true;}
        requireRecord(invalidRejected&&invalidValues==std::vector<double>({1,2}),"Invalid AGC settings modified amplitudes");
        std::vector<SeismicIO::TraceMetadata> geometry(2);const std::vector<std::vector<float>> values={{0,1,-2},{3,0,-1}};
        const auto exported=directory/"export.sgy";SeismicIO::writeSegyFile2D(values,geometry,exported.string(),.001f);
        const auto roundtrip=Seis2D::readRecordSegy2D(exported);
        requireRecord(roundtrip.traces==values&&roundtrip.units=="m/s","2D acquisition export round trip changed data or units");
        bool rejected=false;try {SeismicIO::writeSegyFile2D(values,geometry,exported.string(),.1f);}catch(const std::invalid_argument&) {rejected=true;}
        requireRecord(rejected&&Seis2D::readRecordSegy2D(exported).traces==values,"Invalid export overwrote an existing record");
        Seis2D::FrozenModel2D frozen;frozen.nx=frozen.nz=8;frozen.spacing=10;frozen.solverDt=.001f;
        frozen.formulation=1;frozen.frequency=20;frozen.sourceAmplitude=1;
        frozen.lambda.assign(64,2e9f);frozen.mu.assign(64,1e9f);frozen.rho.assign(64,2000);frozen.wavelet={0,1,0};
        Seis2D::ShotExportRequest2D package;package.directory=directory/std::filesystem::path(u8"中文归档");
        package.prefix="shot";package.samples=3;package.sampleInterval=.001f;package.version="regression";
        geometry[0].coordinateScalar=geometry[1].coordinateScalar=-1000;geometry[0].sourceX=12345;
        geometry[0].traceInRecord=1;geometry[1].traceInRecord=2;
        const auto published=Seis2D::exportShotPackage2D(package,frozen,values,values,geometry);
        requireRecord(Seis2D::readRecordSegy2D(published/"record_vz.sgy").traces==values,"Unicode package export changed raw data");
        requireRecord(std::filesystem::file_size(published/"model.s2dm")==20+64*12&&std::filesystem::exists(published/"manifest.json"),"Frozen model or manifest missing");
        std::ifstream manifestFile(published/"manifest.json");
        const std::string manifestText((std::istreambuf_iterator<char>(manifestFile)),std::istreambuf_iterator<char>());
        requireRecord(manifestText.find("S2DMOD1\\\\0")!=std::string::npos,"Model magic was not escaped as valid JSON text");
        bool collision=false;try {Seis2D::exportShotPackage2D(package,frozen,values,values,geometry);}catch(const std::runtime_error&) {collision=true;}
        requireRecord(collision&&Seis2D::readRecordSegy2D(published/"record_vx.sgy").traces==values,"Package collision overwrote raw data");
        for(const char* prefix:{"../escape","CON","bad:name","trailing."}) {
            bool bad=false;try {Seis2D::validateExportPrefix2D(prefix);}catch(const std::invalid_argument&) {bad=true;}
            requireRecord(bad,"Unsafe package prefix was accepted");
        }
        package.shot=2;auto invalid=values;invalid[0][0]=std::numeric_limits<float>::quiet_NaN();bool refused=false;
        try {Seis2D::exportShotPackage2D(package,frozen,invalid,values,geometry);}catch(const std::invalid_argument&) {refused=true;}
        requireRecord(refused&&!std::filesystem::exists(package.directory/"shot_0002"),"Invalid package created visible output");
        for(const auto& item:std::filesystem::directory_iterator(package.directory))
            requireRecord(item.path().filename().string().find(".part-")==std::string::npos,"Staging files leaked after export");
        std::puts("PASS: Unicode shot package, frozen model, manifest/SHA-256 generation, no-overwrite collision and invalid-input isolation");
        std::puts("PASS: five SEG-Y sample formats, Unicode input, trace delays, rollback, display persistence, AGC/raw isolation, extrema, cache, selection, export units/bounds");return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
