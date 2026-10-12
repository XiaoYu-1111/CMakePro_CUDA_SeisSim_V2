#pragma once
#include <cuda_runtime.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Seis3D {

constexpr int stencilRadius3D = 4;
constexpr std::size_t memoryLimit3D = std::size_t(4) * 1024 * 1024 * 1024;
constexpr int maxReceivers3D = 4096;
constexpr int maxSegySamples3D = 32767;
constexpr int maxSegyIntervalUs3D = 32767; // SEG-Y rev1 signed two-byte positive values.

enum class Field3D : int { Vx, Vy, Vz, Sxx, Syy, Szz, Sxy, Syz, Sxz };
enum class ModelKind3D : int { Uniform, TwoLayer, Sphere, Imported };
enum class SourceKind3D : int { Explosion, Force };

struct SimulationContext3D {
    int nx3D = 96, ny3D = 96, nz3D = 96; // Physical nodes, excluding CPML and ghosts.
    int pml3D = 12;
    bool freeSurface3D = true;
    float spacing3D = 10.0f, cfl3D = 0.85f, dt3D = 0.0f;
    float vp3D = 3000.0f, vs3D = 1700.0f, density3D = 2200.0f;
    float layerVp3D = 4200.0f, layerVs3D = 2400.0f, layerDensity3D = 2500.0f;
    float maxVp3D = 4200.0f, frequency3D = 20.0f;
    ModelKind3D modelKind3D = ModelKind3D::TwoLayer;
    // These dimensions and offsets are derived only by validateContext3D().
    int gx3D = 0, gy3D = 0, gz3D = 0, ox3D = 0, oy3D = 0, oz3D = 0;
    std::size_t cells3D = 0;
};

struct Source3D {
    int x3D = 48, y3D = 48, z3D = 24;
    SourceKind3D kind3D = SourceKind3D::Explosion;
    float amplitude3D = 1.0e8f; // Pa/s for explosion; N/m^3 for body force.
    float direction3D[3] = { 0.0f, 0.0f, 1.0f };
};

struct Receiver3D { int x3D = 0, y3D = 0, z3D = 0; };

struct ViewLayout3D {
    // Offsets/counts are scalar sample counts; GL staging stores two floats per sample (wave, material).
    std::size_t sliceOffset3D[3]{};
    int sliceWidth3D[3]{}, sliceHeight3D[3]{};
    std::size_t volumeOffset3D = 0, totalSamples3D = 0;
    int volumeX3D = 0, volumeY3D = 0, volumeZ3D = 0;
};

struct MemoryBudget3D {
    std::size_t fieldBytes3D = 0, cpmlBytes3D = 0, viewBytes3D = 0, totalBytes3D = 0;
};

struct GPUSimData3D {
    float* field3D[9]{};
    float* inverseDensity3D = nullptr;
    float* lambda3D = nullptr;
    float* mu3D = nullptr;
    // Six derivative memories per axis, stored exclusively in CPML slabs.
    float* cpml3D[3]{};
    std::size_t slabCells3D[3]{};
    float* coefficients3D[3]{}; // Center/half-grid {a,b,1/kappa} per axis node.
    Receiver3D* receivers3D = nullptr;
    float* receiverValues3D = nullptr;
    float* hostReceiverValues3D = nullptr; // Pinned, at most 4096 x 3 floats.
    unsigned int* viewStatistics3D = nullptr; // Wave abs max, non-finite flag, material min/max bits.
    cudaStream_t stream3D = nullptr;
    cudaEvent_t startEvent3D = nullptr, stopEvent3D = nullptr;
    int receiverCount3D = 0;
    std::size_t allocatedBytes3D = 0;
};

// Return the padded flat index; X is contiguous in device memory.
inline std::size_t gridIndex3D(const SimulationContext3D& c, int x, int y, int z) {
    return (std::size_t(z) * c.gy3D + y) * c.gx3D + x;
}

} // namespace Seis3D
