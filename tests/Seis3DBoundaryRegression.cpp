#include "Cuda_Seis3D.cuh"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace Seis3D {
// Fail with the actual CUDA operation rather than accepting an incomplete numerical measurement.
void checkDiagnosticCuda3D(cudaError_t result) {
    if(result!=cudaSuccess) throw std::runtime_error(cudaGetErrorString(result));
}

// Measure collocated velocity magnitude in SI units; energy here is sum(|v|^2), not total elastic energy.
double measureBoundaryCase3D(const char* name,int pml,bool layered,bool reflecting,int sourceDepth) {
    SimulationContext3D c;
    // The axial-symmetry baseline requires a cube; UI defaults may use an elongated receiver grid.
    c.nx3D=c.ny3D=c.nz3D=96;
    c.freeSurface3D=false;c.reflecting3D=reflecting;c.pml3D=pml;
    c.modelKind3D=layered?ModelKind3D::TwoLayer:ModelKind3D::Uniform;
    validateContext3D(c);
    GPUSimData3D g;initializeGpu3D(g,c);
    struct Release3D {GPUSimData3D& g;~Release3D(){releaseGpu3D(g);}} release{g};
    Source3D source;source.x3D=c.nx3D/2;source.y3D=c.ny3D/2;source.z3D=sourceDepth;
    source.kind3D=SourceKind3D::Explosion;
    std::vector<float> v[3];for(auto& field:v) field.resize(c.cells3D);
    double peak=0,last=0;
    for(int step=0;step<1600;++step) {
        runStep3D(g,c,source,step);
        if((step+1)%200) continue;
        checkDiagnosticCuda3D(cudaStreamSynchronize(g.stream3D));
        for(int a=0;a<3;++a) checkDiagnosticCuda3D(cudaMemcpy(v[a].data(),g.field3D[a],c.cells3D*sizeof(float),cudaMemcpyDeviceToHost));
        double physical=0;float physicalMax=0,faceMax[6]{};
        for(int z=4;z<c.gz3D-4;++z) for(int y=4;y<c.gy3D-4;++y) for(int x=4;x<c.gx3D-4;++x) {
            const auto i=(std::size_t(z)*c.gy3D+y)*c.gx3D+x;
            const float vx=.5f*(v[0][i]+v[0][i-1]);
            const float vy=.5f*(v[1][i]+v[1][i-c.gx3D]);
            const float vz=.5f*(v[2][i]+v[2][i-std::size_t(c.gx3D)*c.gy3D]);
            const double energy=double(vx)*vx+double(vy)*vy+double(vz)*vz;
            if(!std::isfinite(energy)) throw std::runtime_error("Non-finite velocity in boundary case");
            const float magnitude=float(std::sqrt(energy));
            const int position[3]={x-c.ox3D,y-c.oy3D,z-c.oz3D};
            const int size[3]={c.nx3D,c.ny3D,c.nz3D};bool inside=true;
            for(int a=0;a<3;++a) {
                if(position[a]<0) {faceMax[2*a]=(std::max)(faceMax[2*a],magnitude);inside=false;}
                if(position[a]>=size[a]) {faceMax[2*a+1]=(std::max)(faceMax[2*a+1],magnitude);inside=false;}
            }
            if(inside) {physical+=energy;physicalMax=(std::max)(physicalMax,magnitude);}
        }
        peak=(std::max)(peak,physical);last=physical;
        if(!layered&&!reflecting&&sourceDepth==48&&step<400) {
            for(int axis=1;axis<3;++axis) for(int side=0;side<2;++side)
                if(std::abs(faceMax[2*axis+side]-faceMax[side])>1e-3f*(std::max)(faceMax[side],1e-20f))
                    throw std::runtime_error("Centered uniform CPML lost axial symmetry at the top/side faces");
        }
        std::printf("%s,%d,%.6f,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",name,step+1,
            (step+1)*c.dt3D,physical,physicalMax,faceMax[0],faceMax[1],faceMax[2],faceMax[3],faceMax[4],faceMax[5]);
        std::fflush(stdout);
    }
    const double ratio=last/(std::max)(peak,1e-30);
    std::printf("SUMMARY %s late/peak velocity-square-sum = %.9g\n",name,ratio);
    return ratio;
}
} // namespace Seis3D

// Compare the production solver's six-face absorption with a lossless reference and a shallow layered source.
int main() {
    try {
        // Exercise dimension validation without allocating large volumes, including overflow inputs.
        for(const auto size:std::vector<std::array<int,3>>{{600,200,200},{300,300,400}}) {
            Seis3D::SimulationContext3D c;c.nx3D=size[0];c.ny3D=size[1];c.nz3D=size[2];Seis3D::validateContext3D(c);
        }
        for(const auto size:std::vector<std::array<int,3>>{{400,400,300},{2147483647,2147483647,2147483647},{-1,96,96}}) {
            Seis3D::SimulationContext3D c;c.nx3D=size[0];c.ny3D=size[1];c.nz3D=size[2];bool rejected=false;
            try{Seis3D::validateContext3D(c);}catch(const std::invalid_argument&){rejected=true;}
            if(!rejected) throw std::runtime_error("Invalid grid bypassed the total-node limit");
        }
        for(const auto size:std::vector<std::array<int,3>>{{96,96,96},{160,160,160},{300,300,300},{300,300,400}}) {
            Seis3D::SimulationContext3D c;c.nx3D=size[0];c.ny3D=size[1];c.nz3D=size[2];
            const auto b=Seis3D::estimateMemory3D(c);constexpr double gib=1073741824.;
            std::printf("BUDGET %d/%d/%d fields %.6f cpml %.6f total %.6f GiB\n",size[0],size[1],size[2],b.fieldBytes3D/gib,b.cpmlBytes3D/gib,b.totalBytes3D/gib);
        }
        int count=0;if(cudaGetDeviceCount(&count)!=cudaSuccess||!count) return 77;
        std::puts("case,step,time_s,physical_v2_sum,physical_v_max,x_min,x_max,y_min,y_max,z_min,z_max");
        const double center=Seis3D::measureBoundaryCase3D("uniform-center-p12",12,false,false,48);
        const double shallow=Seis3D::measureBoundaryCase3D("uniform-shallow-p40",40,false,false,24);
        const double layers=Seis3D::measureBoundaryCase3D("layers-shallow-p40",40,true,false,24);
        const double reflecting=Seis3D::measureBoundaryCase3D("uniform-reflecting",12,false,true,48);
        if(center>1e-6 || shallow>1e-6 || layers>1e-6 || reflecting<.1)
            throw std::runtime_error("Absorbing/reflection late-time contrast regressed");
        std::puts("PASS: total-node validation, finite six-face CPML decay, lossless reflecting reference");
        return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
