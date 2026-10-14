#include "Internal3D.h"
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

namespace Seis3D::Detail3D {
// Read a little-endian model header integer, rejecting truncated input immediately.
std::uint32_t readU32Le3D(std::istream& input) {
    unsigned char b[4]{}; input.read(reinterpret_cast<char*>(b),4);
    if(!input) throw std::runtime_error("Seis3D model file is truncated.");
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}

// Write a portable little-endian model header integer.
void writeU32Le3D(std::ostream& output,std::uint32_t value) {
    const unsigned char b[4]={static_cast<unsigned char>(value),static_cast<unsigned char>(value>>8),
        static_cast<unsigned char>(value>>16),static_cast<unsigned char>(value>>24)};
    output.write(reinterpret_cast<const char*>(b),4);
}

// Validate the complete on-disk model before replacing a live simulation or allocating large buffers.
SimulationContext3D inspectModel3D(const std::filesystem::path& path, SimulationContext3D c) {
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot open Seis3D model.");
    char magic[8]{}; input.read(magic,8);
    const auto version=readU32Le3D(input);
    if(std::memcmp(magic,"S3DMOD1",8)!=0 || (version!=1 && version!=2)) throw std::runtime_error("Not a supported .s3dm model.");
    c.nx3D=int(readU32Le3D(input)); c.ny3D=int(readU32Le3D(input)); c.nz3D=int(readU32Le3D(input));
    c.pml3D=int(readU32Le3D(input)); const auto freeSurface=readU32Le3D(input);
    // Version 1 used a boolean. Version 2 adds code 2 for six reflecting slip walls.
    if(freeSurface>(version==1?1u:2u)) throw std::runtime_error("Invalid model boundary flag.");
    c.freeSurface3D=freeSurface==1;c.reflecting3D=freeSurface==2;
    const auto spacingBits=readU32Le3D(input); std::memcpy(&c.spacing3D,&spacingBits,4);
    c.modelKind3D=ModelKind3D::Imported;
    // Check shape/budget first; only the payload scan establishes the imported model's actual CFL ceiling.
    auto layout=c;layout.manualTimeStep3D=false;validateContext3D(layout);
    if(estimateMemory3D(layout).totalBytes3D>memoryLimit3D) throw std::runtime_error("Imported model exceeds the 4 GiB sandbox budget.");
    const std::size_t planeSize=std::size_t(c.nx3D)*c.ny3D*3;
    if(std::filesystem::file_size(path)!=36+planeSize*c.nz3D*sizeof(float)) throw std::runtime_error("Model payload size does not match dimensions.");
    std::vector<float> plane(planeSize); c.maxVp3D=0;
    for(int z=0;z<c.nz3D;++z) {
        input.read(reinterpret_cast<char*>(plane.data()),std::streamsize(plane.size()*4));
        if(!input) throw std::runtime_error("Cannot read model plane.");
        for(std::size_t i=0;i<planeSize;i+=3) {
            const float vp=plane[i],vs=plane[i+1],rho=plane[i+2];
            if(!std::isfinite(vp)||!std::isfinite(vs)||!std::isfinite(rho)||vp<=0||vp>30000||vs<0||vs>=vp
                ||vp*vp<=(4.0f/3.0f)*vs*vs||rho<=0||rho>50000) throw std::runtime_error("Model contains an invalid material cell.");
            c.maxVp3D=(std::max)(c.maxVp3D,vp);
        }
    }
    validateContext3D(c); return c;
}

// Read a validated model one plane at a time, with no complete CPU copy of a 300^3 volume.
void importModel3D(Seis3D_State& s,const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary); input.seekg(36);
    std::vector<float> plane(std::size_t(s.context3D.nx3D)*s.context3D.ny3D*3);
    for(int z=0;z<s.context3D.nz3D;++z) {
        input.read(reinterpret_cast<char*>(plane.data()),std::streamsize(plane.size()*4));
        if(!input) throw std::runtime_error("Seis3D model changed or became unreadable during import.");
        uploadModelPlane3D(s.gpu3D,s.context3D,z,plane.data());
    }
    finishModelUpload3D(s.gpu3D,s.context3D);
}

// Write a streaming .s3dm model to a new file, preserving an existing file on failed writes.
void exportModel3D(Seis3D_State& s) {
    const auto path=uiPath3D(s.modelExportPath3D);
    if(path.empty()||std::filesystem::exists(path)) throw std::runtime_error("Choose a new model filename; the destination already exists or is empty.");
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    const auto temporary=std::filesystem::path(path.wstring()+L".part");
    try {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        if(!output) throw std::runtime_error("Cannot create Seis3D model file.");
        output.write("S3DMOD1",8); writeU32Le3D(output,s.context3D.reflecting3D?2:1);
        const auto& c=s.context3D;
        writeU32Le3D(output,c.nx3D); writeU32Le3D(output,c.ny3D); writeU32Le3D(output,c.nz3D);
        writeU32Le3D(output,c.pml3D); writeU32Le3D(output,c.reflecting3D?2:c.freeSurface3D?1:0);
        std::uint32_t spacing=0; std::memcpy(&spacing,&c.spacing3D,4); writeU32Le3D(output,spacing);
        std::vector<float> plane(std::size_t(c.nx3D)*c.ny3D*3);
        for(int z=0;z<c.nz3D;++z) {
            downloadModelPlane3D(s.gpu3D,c,z,plane.data()); output.write(reinterpret_cast<const char*>(plane.data()),std::streamsize(plane.size()*4));
            if(!output) throw std::runtime_error("Seis3D model write failed; check disk space.");
        }
        output.close(); if(!output) throw std::runtime_error("Cannot finalize model file.");
        std::filesystem::rename(temporary,path);
    } catch(...) { std::error_code ignored; std::filesystem::remove(temporary,ignored); throw; }
    s.message3D="3D model exported: "+pathToUtf8(path);
}


} // namespace Seis3D::Detail3D
