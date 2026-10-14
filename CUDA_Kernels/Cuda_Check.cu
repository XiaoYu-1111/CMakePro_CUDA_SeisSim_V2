#include <iostream>
#include <string.h>
#include <algorithm> // 确保 std::max 可用

#include "Cuda_Check.cuh"
#include <device_launch_parameters.h>

#include <thrust/reduce.h>
#include <thrust/device_ptr.h>
#include <thrust/execution_policy.h>

#include <cuda_runtime.h>
#include <curand_kernel.h>

// 防御性定义 M_PI，防止 MSVC 环境下 math 常量未导出的编译错误
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// =============================================================================
// 1. 基础硬件测试模块
// =============================================================================

// 加法核函数
// Adds vector elements for the startup CUDA smoke test.
__global__ void vectorAddKernel(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

// 辅助函数：根据架构计算核心数
// Maps a compute-capability version to the estimated CUDA cores per multiprocessor.
int convertSmVersionToCudaCoreCount(int major, int minor) {
    switch (major) {
    case 7:  return 64; // Volta, Turing (7.0, 7.5)
    case 8:  return (minor == 0) ? 64 : 128; // Ampere (8.0=64, 8.6/8.9=128)
    case 9:  return 128; // Hopper
    default: return 128;
    }
}

// 获取 GPU 设备信息
// Returns the properties of the active CUDA device for startup checks and diagnostics.
extern "C" GpuInfo getCudaDeviceInfo() {
    GpuInfo info;
    memset(&info, 0, sizeof(info));
    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount == 0) return info;

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);

    // 使用具有跨平台移植性的 snprintf
    snprintf(info.name, sizeof(info.name), "%s", prop.name);
    info.computeCapabilityMajor = prop.major;
    info.computeCapabilityMinor = prop.minor;
    info.totalMem = prop.totalGlobalMem;
    info.multiProcessorCount = prop.multiProcessorCount;
    info.clockRate = prop.clockRate;

    // 计算核心数：SMs * CoresPerSM
    info.cudaCores = prop.multiProcessorCount * convertSmVersionToCudaCoreCount(prop.major, prop.minor);
    info.success = true;
    return info;
}

// Verifies a minimal host-to-device, kernel, and device-to-host execution path.
extern "C" bool runCudaTest(float* hostA, float* hostB, float* hostC, int elementCount) {
    float* d_a = nullptr, * d_b = nullptr, * d_c = nullptr;
    size_t bytes = elementCount * sizeof(float);

    if (cudaMalloc(&d_a, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&d_b, bytes) != cudaSuccess) { cudaFree(d_a); return false; }
    if (cudaMalloc(&d_c, bytes) != cudaSuccess) { cudaFree(d_a); cudaFree(d_b); return false; }

    cudaMemcpy(d_a, hostA, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, hostB, bytes, cudaMemcpyHostToDevice);

    int threads = 256;
    int blocks = (elementCount + threads - 1) / threads;
    vectorAddKernel << <blocks, threads >> > (d_a, d_b, d_c, elementCount);

    if (cudaDeviceSynchronize() != cudaSuccess) {
        cudaFree(d_a); cudaFree(d_b); cudaFree(d_c);
        return false;
    }

    cudaMemcpy(hostC, d_c, bytes, cudaMemcpyDeviceToHost);

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    return true;
}

// =============================================================================
// 2. 生命游戏模块
// =============================================================================

static curandState* d_states = nullptr; // 随机数状态缓冲区

// 释放可选演示模块的随机状态，允许退出清理重复调用。
extern "C" void releaseCudaLife() {
    cudaFree(d_states);
    d_states = nullptr;
}

// Initializes one CURAND state per cell for the life-game demonstration.
__global__ void initializeRandomStatesKernel(curandState* states, unsigned long seed, int w, int h) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < w * h) {
        curand_init(seed, idx, 0, &states[idx]);
    }
}

// Allocates and seeds the random states used by the standalone life-game demo.
extern "C" void initializeCudaLife(int w, int h) {
    if (d_states) cudaFree(d_states);
    cudaMalloc(&d_states, w * h * sizeof(curandState));

    int threads = 256;
    int blocks = (w * h + threads - 1) / threads;
    initializeRandomStatesKernel << <blocks, threads >> > (d_states, (unsigned long)time(0), w, h);
    cudaDeviceSynchronize();
}

// Writes a random binary state into each life-game cell.
__global__ void seedLifeGridKernel(uint8_t* world, curandState* states, float density, int w, int h) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < w * h) {
        float r = curand_uniform(&states[idx]);
        world[idx] = (r < density) ? 1 : 0;
    }
}

// Initializes every cell using the requested live-cell density.
extern "C" void seedCudaLife(uint8_t* d_world, int w, int h, float density) {
    int n = w * h;
    seedLifeGridKernel << <(n + 255) / 256, 256 >> > (d_world, d_states, density, w, h);
    cudaDeviceSynchronize();
}

// Applies the configured birth/survival rules and decays the per-cell heat map.
__global__ void updateLifeGridKernel(const uint8_t* current, uint8_t* next, float* heatMap, int w, int h, float dt,
    bool paused, float decay, int b_mask, int s_mask) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;
    int idx = y * w + x;

    if (!paused) {
        int neighbors = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                neighbors += current[((y + dy + h) % h) * w + ((x + dx + w) % w)];
            }
        }
        uint8_t alive = current[idx];
        if (alive) {
            next[idx] = (s_mask & (1 << neighbors)) ? 1 : 0;
        }
        else {
            next[idx] = (b_mask & (1 << neighbors)) ? 1 : 0;
        }
    }

    float hVal = heatMap[idx];
    if (next[idx] == 1) {
        hVal = 1.0f;
    }
    else {
        hVal *= decay;
        if (hVal < 0.005f) hVal = 0.0f;
    }
    heatMap[idx] = hVal;
}

// Advances one life-game frame and updates the heat-map buffer.
extern "C" void updateCudaLife(uint8_t* d_current, uint8_t* d_next, float* d_heatMap, int w, int h, float deltaTime,
    bool paused, float decay, int b_mask, int s_mask) {
    dim3 block(16, 16);
    dim3 grid((w + 15) / 16, (h + 15) / 16);
    updateLifeGridKernel << <grid, block >> > (d_current, d_next, d_heatMap, w, h, deltaTime, paused, decay, b_mask, s_mask);
}

// Applies a circular brush to cells within the image bounds.
__global__ void paintLifeGridKernel(uint8_t* world, float* heatMap, int w, int h, int mx, int my, int radius, bool erase) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;

    float dist = sqrtf(powf(x - mx, 2) + powf(y - my, 2));
    if (dist < radius) {
        int idx = y * w + x;
        world[idx] = erase ? 0 : 1;
        if (!erase) heatMap[idx] = 1.0f;
    }
}

// Paints or erases cells within the requested circular brush on the GPU.
extern "C" void paintCudaLife(uint8_t* d_world, float* d_heat, int w, int h, int mx, int my, int radius, bool erase) {
    dim3 block(16, 16);
    dim3 grid((w + 15) / 16, (h + 15) / 16);
    paintLifeGridKernel << <grid, block >> > (d_world, d_heat, w, h, mx, my, radius, erase);
}

// Returns the number of live cells in the current life-game grid.
extern "C" int getCudaLifePopulation(uint8_t* d_world, int w, int h) {
    thrust::device_ptr<uint8_t> ptr(d_world);
    try {
        return thrust::reduce(thrust::device, ptr, ptr + (w * h), (int)0, thrust::plus<int>());
    }
    catch (...) {
        return 0;
    }
}

// =============================================================================
// 3. 地震数值模拟核心核函数 (波场演化)
// =============================================================================

// --- Type 1 PML Stress Update ---
// Updates split stress fields using the full-domain split-field PML scheme.
__global__ void updateType1StressKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= 4 && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;
        float inv_dt = g.inv_dt;

        float d_z = g.d_dz[i];
        float d_z_h = g.d_dz_half[i];
        float d_x = g.d_dx[j];
        float d_x_h = g.d_dx_half[j];

        float dvx_dx = g.c1_h * (g.d_vx_1[k + 1] + g.d_vx_2[k + 1] - g.d_vx_1[k] - g.d_vx_2[k])
            + g.c2_h * (g.d_vx_1[k + 2] + g.d_vx_2[k + 2] - g.d_vx_1[k - 1] - g.d_vx_2[k - 1])
            + g.c3_h * (g.d_vx_1[k + 3] + g.d_vx_2[k + 3] - g.d_vx_1[k - 2] - g.d_vx_2[k - 2])
            + g.c4_h * (g.d_vx_1[k + 4] + g.d_vx_2[k + 4] - g.d_vx_1[k - 3] - g.d_vx_2[k - 3]);

        float dvz_dz = g.c1_h * (g.d_vz_1[k] + g.d_vz_2[k] - g.d_vz_1[k - g.NX] - g.d_vz_2[k - g.NX])
            + g.c2_h * (g.d_vz_1[k + g.NX] + g.d_vz_2[k + g.NX] - g.d_vz_1[k - 2 * g.NX] - g.d_vz_2[k - 2 * g.NX])
            + g.c3_h * (g.d_vz_1[k + 2 * g.NX] + g.d_vz_2[k + 2 * g.NX] - g.d_vz_1[k - 3 * g.NX] - g.d_vz_2[k - 3 * g.NX])
            + g.c4_h * (g.d_vz_1[k + 3 * g.NX] + g.d_vz_2[k + 3 * g.NX] - g.d_vz_1[k - 4 * g.NX] - g.d_vz_2[k - 4 * g.NX]);

        g.d_sxx_1[k] = (g.d_sxx_1[k] * (inv_dt - d_x_h) + 0.5f * (g.d_lambda2mu[k] + g.d_lambda2mu[k + 1]) * dvx_dx) / (inv_dt + d_x_h);
        g.d_sxx_2[k] = (g.d_sxx_2[k] * (inv_dt - d_z) + 0.5f * (g.d_lambda[k] + g.d_lambda[k + 1]) * dvz_dz) / (inv_dt + d_z);
        g.d_szz_1[k] = (g.d_szz_1[k] * (inv_dt - d_x_h) + 0.5f * (g.d_lambda[k] + g.d_lambda[k + 1]) * dvx_dx) / (inv_dt + d_x_h);
        g.d_szz_2[k] = (g.d_szz_2[k] * (inv_dt - d_z) + 0.5f * (g.d_lambda2mu[k] + g.d_lambda2mu[k + 1]) * dvz_dz) / (inv_dt + d_z);

        float dvz_dx = g.c1_h * (g.d_vz_1[k] + g.d_vz_2[k] - g.d_vz_1[k - 1] - g.d_vz_2[k - 1])
            + g.c2_h * (g.d_vz_1[k + 1] + g.d_vz_2[k + 1] - g.d_vz_1[k - 2] - g.d_vz_2[k - 2])
            + g.c3_h * (g.d_vz_1[k + 2] + g.d_vz_2[k + 2] - g.d_vz_1[k - 3] - g.d_vz_2[k - 3])
            + g.c4_h * (g.d_vz_1[k + 3] + g.d_vz_2[k + 3] - g.d_vz_1[k - 4] - g.d_vz_2[k - 4]);

        float dvx_dz = g.c1_h * (g.d_vx_1[k + g.NX] + g.d_vx_2[k + g.NX] - g.d_vx_1[k] - g.d_vx_2[k])
            + g.c2_h * (g.d_vx_1[k + 2 * g.NX] + g.d_vx_2[k + 2 * g.NX] - g.d_vx_1[k - g.NX] - g.d_vx_2[k - g.NX])
            + g.c3_h * (g.d_vx_1[k + 3 * g.NX] + g.d_vx_2[k + 3 * g.NX] - g.d_vx_1[k - 2 * g.NX] - g.d_vx_2[k - 2 * g.NX])
            + g.c4_h * (g.d_vx_1[k + 4 * g.NX] + g.d_vx_2[k + 4 * g.NX] - g.d_vx_1[k - 3 * g.NX] - g.d_vx_2[k - 3 * g.NX]);

        g.d_sxz_1[k] = (g.d_sxz_1[k] * (inv_dt - d_x) + 0.5f * (g.d_mu[k] + g.d_mu[k + g.NX]) * dvz_dx) / (inv_dt + d_x);
        g.d_sxz_2[k] = (g.d_sxz_2[k] * (inv_dt - d_z_h) + 0.5f * (g.d_mu[k] + g.d_mu[k + g.NX]) * dvx_dz) / (inv_dt + d_z_h);
    }
}

// --- Type 1 PML Velocity Update ---
// Updates split particle-velocity fields for the full-domain PML scheme.
__global__ void updateType1VelocityKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= 4 && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;
        float inv_dt = g.inv_dt;

        float d_z = g.d_dz[i];
        float d_z_h = g.d_dz_half[i]; // 已成功修复：重新引入被删除的 d_z_h
        float d_x = g.d_dx[j];
        float d_x_h = g.d_dx_half[j];
        float cur_rho = g.d_rho[k];

        float dsigxx_dx = g.c1_h * (g.d_sxx_1[k] + g.d_sxx_2[k] - g.d_sxx_1[k - 1] - g.d_sxx_2[k - 1])
            + g.c2_h * (g.d_sxx_1[k + 1] + g.d_sxx_2[k + 1] - g.d_sxx_1[k - 2] - g.d_sxx_2[k - 2])
            + g.c3_h * (g.d_sxx_1[k + 2] + g.d_sxx_2[k + 2] - g.d_sxx_1[k - 3] - g.d_sxx_2[k - 3])
            + g.c4_h * (g.d_sxx_1[k + 3] + g.d_sxx_2[k + 3] - g.d_sxx_1[k - 4] - g.d_sxx_2[k - 4]);

        float dsigxz_dz = g.c1_h * (g.d_sxz_1[k] + g.d_sxz_2[k] - g.d_sxz_1[k - g.NX] - g.d_sxz_2[k - g.NX])
            + g.c2_h * (g.d_sxz_1[k + g.NX] + g.d_sxz_2[k + g.NX] - g.d_sxz_1[k - 2 * g.NX] - g.d_sxz_2[k - 2 * g.NX])
            + g.c3_h * (g.d_sxz_1[k + 2 * g.NX] + g.d_sxz_2[k + 2 * g.NX] - g.d_sxz_1[k - 3 * g.NX] - g.d_sxz_2[k - 3 * g.NX])
            + g.c4_h * (g.d_sxz_1[k + 3 * g.NX] + g.d_sxz_2[k + 3 * g.NX] - g.d_sxz_1[k - 4 * g.NX] - g.d_sxz_2[k - 4 * g.NX]);

        g.d_vx_1[k] = (g.d_vx_1[k] * (inv_dt - d_x) + dsigxx_dx / cur_rho) / (inv_dt + d_x);
        g.d_vx_2[k] = (g.d_vx_2[k] * (inv_dt - d_z) + dsigxz_dz / cur_rho) / (inv_dt + d_z);

        float dsigxz_dx = g.c1_h * (g.d_sxz_1[k + 1] + g.d_sxz_2[k + 1] - g.d_sxz_1[k] - g.d_sxz_2[k])
            + g.c2_h * (g.d_sxz_1[k + 2] + g.d_sxz_2[k + 2] - g.d_sxz_1[k - 1] - g.d_sxz_2[k - 1])
            + g.c3_h * (g.d_sxz_1[k + 3] + g.d_sxz_2[k + 3] - g.d_sxz_1[k - 2] - g.d_sxz_2[k - 2])
            + g.c4_h * (g.d_sxz_1[k + 4] + g.d_sxz_2[k + 4] - g.d_sxz_1[k - 3] - g.d_sxz_2[k - 3]);

        float dsigzz_dz = g.c1_h * (g.d_szz_1[k + g.NX] + g.d_szz_2[k + g.NX] - g.d_szz_1[k] - g.d_szz_2[k])
            + g.c2_h * (g.d_szz_1[k + 2 * g.NX] + g.d_szz_2[k + 2 * g.NX] - g.d_szz_1[k - g.NX] - g.d_szz_2[k - g.NX])
            + g.c3_h * (g.d_szz_1[k + 3 * g.NX] + g.d_szz_2[k + 3 * g.NX] - g.d_szz_1[k - 2 * g.NX] - g.d_szz_2[k - 2 * g.NX])
            + g.c4_h * (g.d_szz_1[k + 4 * g.NX] + g.d_szz_2[k + 4 * g.NX] - g.d_szz_1[k - 3 * g.NX] - g.d_szz_2[k - 3 * g.NX]);

        float rho_avg = 0.25f * (g.d_rho[k] + g.d_rho[k + 1] + g.d_rho[k + g.NX] + g.d_rho[k + g.NX + 1]);
        g.d_vz_1[k] = (g.d_vz_1[k] * (inv_dt - d_x_h) + dsigxz_dx / rho_avg) / (inv_dt + d_x_h);
        g.d_vz_2[k] = (g.d_vz_2[k] * (inv_dt - d_z_h) + dsigzz_dz / rho_avg) / (inv_dt + d_z_h);
    }
}

// --- Type 2 PML/FDM Hybrid Stress Update ---
// Updates stress fields for the interior-FDM and boundary-PML hybrid scheme.
__global__ void updateType2StressKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= 4 && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;
        int row_dn1 = (i + 1) * g.NX; int row_dn2 = (i + 2) * g.NX; int row_dn3 = (i + 3) * g.NX; int row_dn4 = (i + 4) * g.NX;
        int row_up1 = (i - 1) * g.NX; int row_up2 = (i - 2) * g.NX; int row_up3 = (i - 3) * g.NX; int row_up4 = (i - 4) * g.NX;

        float dvz_dx = g.c1_h * (g.d_vz[k] - g.d_vz[k - 1]) + g.c2_h * (g.d_vz[k + 1] - g.d_vz[k - 2])
            + g.c3_h * (g.d_vz[k + 2] - g.d_vz[k - 3]) + g.c4_h * (g.d_vz[k + 3] - g.d_vz[k - 4]);

        float dvx_dz = g.c1_h * (g.d_vx[row_dn1 + j] - g.d_vx[k]) + g.c2_h * (g.d_vx[row_dn2 + j] - g.d_vx[row_up1 + j])
            + g.c3_h * (g.d_vx[row_dn3 + j] - g.d_vx[row_up2 + j]) + g.c4_h * (g.d_vx[row_dn4 + j] - g.d_vx[row_up3 + j]);

        float dvx_dx = g.c1_h * (g.d_vx[k + 1] - g.d_vx[k]) + g.c2_h * (g.d_vx[k + 2] - g.d_vx[k - 1])
            + g.c3_h * (g.d_vx[k + 3] - g.d_vx[k - 2]) + g.c4_h * (g.d_vx[k + 4] - g.d_vx[k - 3]);

        float dvz_dz = g.c1_h * (g.d_vz[k] - g.d_vz[row_up1 + j]) + g.c2_h * (g.d_vz[row_dn1 + j] - g.d_vz[row_up2 + j])
            + g.c3_h * (g.d_vz[row_dn2 + j] - g.d_vz[row_up3 + j]) + g.c4_h * (g.d_vz[row_dn3 + j] - g.d_vz[row_up4 + j]);

        bool is_pml = (i < g.npml || i >= g.NZ - g.npml || j < g.npml || j >= g.NX - g.npml);

        if (!is_pml) {
            g.d_sigmaxz[k] += g.dt * g.d_mu_z_flat[k] * (dvz_dx + dvx_dz);
            g.d_sigmaxx[k] += g.dt * (g.d_lambda2mu_x_flat[k] * dvx_dx + g.d_lambda_x_flat[k] * dvz_dz);
            g.d_sigmazz[k] += g.dt * (g.d_lambda_x_flat[k] * dvx_dx + g.d_lambda2mu_x_flat[k] * dvz_dz);
        }
        else {
            float inv_dt = g.inv_dt;
            float d_z_val = g.d_dz[i];
            float d_z_half_val = g.d_dz_half[i];
            float d_x_val = g.d_dx[j];
            float d_x_half_val = g.d_dx_half[j];

            g.d_sxz_1[k] = (g.d_sxz_1[k] * (inv_dt - d_x_val) + g.d_mu_z_flat[k] * dvz_dx) / (inv_dt + d_x_val);
            g.d_sxz_2[k] = (g.d_sxz_2[k] * (inv_dt - d_z_half_val) + g.d_mu_z_flat[k] * dvx_dz) / (inv_dt + d_z_half_val);

            g.d_sxx_1[k] = (g.d_sxx_1[k] * (inv_dt - d_x_half_val) + g.d_lambda2mu_x_flat[k] * dvx_dx) / (inv_dt + d_x_half_val);
            g.d_szz_1[k] = (g.d_szz_1[k] * (inv_dt - d_x_half_val) + g.d_lambda_x_flat[k] * dvx_dx) / (inv_dt + d_x_half_val);

            g.d_sxx_2[k] = (g.d_sxx_2[k] * (inv_dt - d_z_val) + g.d_lambda_x_flat[k] * dvz_dz) / (inv_dt + d_z_val);
            g.d_szz_2[k] = (g.d_szz_2[k] * (inv_dt - d_z_val) + g.d_lambda2mu_x_flat[k] * dvz_dz) / (inv_dt + d_z_val);

            g.d_sigmaxz[k] = g.d_sxz_1[k] + g.d_sxz_2[k];
            g.d_sigmaxx[k] = g.d_sxx_1[k] + g.d_sxx_2[k];
            g.d_sigmazz[k] = g.d_szz_1[k] + g.d_szz_2[k];
        }
    }
}

// --- Type 2 PML/FDM Hybrid Velocity Update ---
// Updates velocity fields for the interior-FDM and boundary-PML hybrid scheme.
__global__ void updateType2VelocityKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= 4 && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;
        int row_dn1 = (i + 1) * g.NX; int row_dn2 = (i + 2) * g.NX; int row_dn3 = (i + 3) * g.NX; int row_dn4 = (i + 4) * g.NX;
        int row_up1 = (i - 1) * g.NX; int row_up2 = (i - 2) * g.NX; int row_up3 = (i - 3) * g.NX; int row_up4 = (i - 4) * g.NX;

        float dsigxz_dx = g.c1_h * (g.d_sigmaxz[k + 1] - g.d_sigmaxz[k]) + g.c2_h * (g.d_sigmaxz[k + 2] - g.d_sigmaxz[k - 1])
            + g.c3_h * (g.d_sigmaxz[k + 3] - g.d_sigmaxz[k - 2]) + g.c4_h * (g.d_sigmaxz[k + 4] - g.d_sigmaxz[k - 3]);

        float dsigzz_dz = g.c1_h * (g.d_sigmazz[row_dn1 + j] - g.d_sigmazz[k]) + g.c2_h * (g.d_sigmazz[row_dn2 + j] - g.d_sigmazz[row_up1 + j])
            + g.c3_h * (g.d_sigmazz[row_dn3 + j] - g.d_sigmazz[row_up2 + j]) + g.c4_h * (g.d_sigmazz[row_dn4 + j] - g.d_sigmazz[row_up3 + j]);

        float dsigxx_dx = g.c1_h * (g.d_sigmaxx[k] - g.d_sigmaxx[k - 1]) + g.c2_h * (g.d_sigmaxx[k + 1] - g.d_sigmaxx[k - 2])
            + g.c3_h * (g.d_sigmaxx[k + 2] - g.d_sigmaxx[k - 3]) + g.c4_h * (g.d_sigmaxx[k + 3] - g.d_sigmaxx[k - 4]);

        float dsigxz_dz = g.c1_h * (g.d_sigmaxz[k] - g.d_sigmaxz[row_up1 + j]) + g.c2_h * (g.d_sigmaxz[row_dn1 + j] - g.d_sigmaxz[row_up2 + j])
            + g.c3_h * (g.d_sigmaxz[row_dn2 + j] - g.d_sigmaxz[row_up3 + j]) + g.c4_h * (g.d_sigmaxz[row_dn3 + j] - g.d_sigmaxz[row_up4 + j]);

        bool is_pml = (i < g.npml || i >= g.NZ - g.npml || j < g.npml || j >= g.NX - g.npml);

        if (!is_pml) {
            g.d_vz[k] += g.dt * (dsigxz_dx + dsigzz_dz) / g.d_rho_x_z_flat[k];
            g.d_vx[k] += g.dt * (dsigxx_dx + dsigxz_dz) / g.d_rho_orig_flat[k];
        }
        else {
            float inv_dt = g.inv_dt;
            float d_z_val = g.d_dz[i];
            float d_z_half_val = g.d_dz_half[i];
            float d_x_val = g.d_dx[j];
            float d_x_half_val = g.d_dx_half[j];

            g.d_vz_1[k] = (g.d_vz_1[k] * (inv_dt - d_x_half_val) + dsigxz_dx / g.d_rho_x_z_flat[k]) / (inv_dt + d_x_half_val);
            g.d_vz_2[k] = (g.d_vz_2[k] * (inv_dt - d_z_half_val) + dsigzz_dz / g.d_rho_x_z_flat[k]) / (inv_dt + d_z_half_val);

            g.d_vx_1[k] = (g.d_vx_1[k] * (inv_dt - d_x_val) + dsigxx_dx / g.d_rho_orig_flat[k]) / (inv_dt + d_x_val);
            g.d_vx_2[k] = (g.d_vx_2[k] * (inv_dt - d_z_val) + dsigxz_dz / g.d_rho_orig_flat[k]) / (inv_dt + d_z_val);

            g.d_vx[k] = g.d_vx_1[k] + g.d_vx_2[k];
            g.d_vz[k] = g.d_vz_1[k] + g.d_vz_2[k];
        }
    }
}

// --- Type 3 Free Surface Stress Update ---
// Updates split stress fields while enforcing the traction-free surface condition.
__global__ void updateType3StressKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= g.fs_idx && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;
        int row_dn1 = (i + 1) * g.NX; int row_dn2 = (i + 2) * g.NX;
        int row_dn3 = (i + 3) * g.NX; int row_dn4 = (i + 4) * g.NX;
        int row_up1 = (i - 1) * g.NX; int row_up2 = (i - 2) * g.NX; int row_up3 = (i - 3) * g.NX; int row_up4 = (i - 4) * g.NX;

        float d_z_val = (i <= g.fs_idx + 5 && g.upFlag) ? 0.0f : g.d_dz[i];
        float d_z_half_val = (i <= g.fs_idx + 5 && g.upFlag) ? 0.0f : g.d_dz_half[i];
        float d_x_val = g.d_dx[j];
        float d_x_half_val = g.d_dx_half[j];
        float inv_dt = g.inv_dt;

        if (i == g.fs_idx) {
            g.d_szz_1[k] = 0.0f; g.d_szz_2[k] = 0.0f;

            float dvx_dx = g.c1_h * (g.d_vx_1[k + 1] + g.d_vx_2[k + 1] - g.d_vx_1[k] - g.d_vx_2[k])
                + g.c2_h * (g.d_vx_1[k + 2] + g.d_vx_2[k + 2] - g.d_vx_1[k - 1] - g.d_vx_2[k - 1])
                + g.c3_h * (g.d_vx_1[k + 3] + g.d_vx_2[k + 3] - g.d_vx_1[k - 2] - g.d_vx_2[k - 2])
                + g.c4_h * (g.d_vx_1[k + 4] + g.d_vx_2[k + 4] - g.d_vx_1[k - 3] - g.d_vx_2[k - 3]);

            // The free-surface row still lies inside the left/right PML. Keep
            // the x-split damping here, using the traction-free effective
            // modulus, instead of bypassing the side absorber at the corner.
            g.d_sxx_1[k] = (g.d_sxx_1[k] * (inv_dt - d_x_half_val) + g.d_dp_flat[j] * dvx_dx)
                / (inv_dt + d_x_half_val);
            g.d_sxx_2[k] = 0.0f;

            float dvz_dx = g.c1_h * (g.d_vz_1[k] + g.d_vz_2[k] - g.d_vz_1[k - 1] - g.d_vz_2[k - 1])
                + g.c2_h * (g.d_vz_1[k + 1] + g.d_vz_2[k + 1] - g.d_vz_1[k - 2] - g.d_vz_2[k - 2]);

            float dvx_dz = g.c1_h * (g.d_vx_1[row_dn1 + j] + g.d_vx_2[row_dn1 + j] - g.d_vx_1[k] - g.d_vx_2[k])
                + g.c2_h * (g.d_vx_1[row_dn2 + j] + g.d_vx_2[row_dn2 + j] - g.d_vx_1[row_up1 + j] - g.d_vx_2[row_up1 + j]);

            g.d_sxz_1[k] = (g.d_sxz_1[k] * (inv_dt - d_x_val) + 0.5f * (g.d_mu[k] + g.d_mu[row_dn1 + j]) * dvz_dx) / (inv_dt + d_x_val);
            g.d_sxz_2[k] = (g.d_sxz_2[k] * (inv_dt - d_z_half_val) + 0.5f * (g.d_mu[k] + g.d_mu[row_dn1 + j]) * dvx_dz) / (inv_dt + d_z_half_val);
        }
        else {
            float dvx_dx = g.c1_h * (g.d_vx_1[k + 1] + g.d_vx_2[k + 1] - g.d_vx_1[k] - g.d_vx_2[k])
                + g.c2_h * (g.d_vx_1[k + 2] + g.d_vx_2[k + 2] - g.d_vx_1[k - 1] - g.d_vx_2[k - 1])
                + g.c3_h * (g.d_vx_1[k + 3] + g.d_vx_2[k + 3] - g.d_vx_1[k - 2] - g.d_vx_2[k - 2])
                + g.c4_h * (g.d_vx_1[k + 4] + g.d_vx_2[k + 4] - g.d_vx_1[k - 3] - g.d_vx_2[k - 3]);

            float dvz_dz = g.c1_h * (g.d_vz_1[k] + g.d_vz_2[k] - g.d_vz_1[row_up1 + j] - g.d_vz_2[row_up1 + j])
                + g.c2_h * (g.d_vz_1[row_dn1 + j] + g.d_vz_2[row_dn1 + j] - g.d_vz_1[row_up2 + j] - g.d_vz_2[row_up2 + j])
                + g.c3_h * (g.d_vz_1[row_dn2 + j] + g.d_vz_2[row_dn2 + j] - g.d_vz_1[row_up3 + j] - g.d_vz_2[row_up3 + j])
                + g.c4_h * (g.d_vz_1[row_dn3 + j] + g.d_vz_2[row_dn3 + j] - g.d_vz_1[row_up4 + j] - g.d_vz_2[row_up4 + j]);

            g.d_sxx_1[k] = (g.d_sxx_1[k] * (inv_dt - d_x_half_val) + 0.5f * (g.d_lambda2mu[k] + g.d_lambda2mu[k + 1]) * dvx_dx) / (inv_dt + d_x_half_val);
            g.d_sxx_2[k] = (g.d_sxx_2[k] * (inv_dt - d_z_val) + 0.5f * (g.d_lambda[k] + g.d_lambda[k + 1]) * dvz_dz) / (inv_dt + d_z_val);

            g.d_szz_1[k] = (g.d_szz_1[k] * (inv_dt - d_x_half_val) + 0.5f * (g.d_lambda[k] + g.d_lambda[k + 1]) * dvx_dx) / (inv_dt + d_x_half_val);
            g.d_szz_2[k] = (g.d_szz_2[k] * (inv_dt - d_z_val) + 0.5f * (g.d_lambda2mu[k] + g.d_lambda2mu[k + 1]) * dvz_dz) / (inv_dt + d_z_val);

            float dvz_dx = g.c1_h * (g.d_vz_1[k] + g.d_vz_2[k] - g.d_vz_1[k - 1] - g.d_vz_2[k - 1])
                + g.c2_h * (g.d_vz_1[k + 1] + g.d_vz_2[k + 1] - g.d_vz_1[k - 2] - g.d_vz_2[k - 2])
                + g.c3_h * (g.d_vz_1[k + 2] + g.d_vz_2[k + 2] - g.d_vz_1[k - 3] - g.d_vz_2[k - 3])
                + g.c4_h * (g.d_vz_1[k + 3] + g.d_vz_2[k + 3] - g.d_vz_1[k - 4] - g.d_vz_2[k - 4]);

            float dvx_dz = g.c1_h * (g.d_vx_1[row_dn1 + j] + g.d_vx_2[row_dn1 + j] - g.d_vx_1[k] - g.d_vx_2[k])
                + g.c2_h * (g.d_vx_1[row_dn2 + j] + g.d_vx_2[row_dn2 + j] - g.d_vx_1[row_up1 + j] - g.d_vx_2[row_up1 + j])
                + g.c3_h * (g.d_vx_1[row_dn3 + j] + g.d_vx_2[row_dn3 + j] - g.d_vx_1[row_up2 + j] - g.d_vx_2[row_up2 + j])
                + g.c4_h * (g.d_vx_1[row_dn4 + j] + g.d_vx_2[row_dn4 + j] - g.d_vx_1[row_up3 + j] - g.d_vx_2[row_up3 + j]);

            g.d_sxz_1[k] = (g.d_sxz_1[k] * (inv_dt - d_x_val) + 0.5f * (g.d_mu[k] + g.d_mu[row_dn1 + j]) * dvz_dx) / (inv_dt + d_x_val);
            g.d_sxz_2[k] = (g.d_sxz_2[k] * (inv_dt - d_z_half_val) + 0.5f * (g.d_mu[k] + g.d_mu[row_dn1 + j]) * dvx_dz) / (inv_dt + d_z_half_val);
        }
    }
}

// --- Type 3 Stress Mirroring ---
// Mirrors stress values across the free surface to provide antisymmetric stencil samples.
__global__ void mirrorType3StressKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (j >= 4 && j < g.NX - 4) {
        for (int l = 1; l <= 4; ++l) {
            int idx_above = (g.fs_idx - l) * g.NX + j;
            int idx_below = (g.fs_idx + l) * g.NX + j;

            if (idx_above >= 0 && idx_below < g.total_grid) {
                g.d_szz_1[idx_above] = -g.d_szz_1[idx_below];
                g.d_szz_2[idx_above] = -g.d_szz_2[idx_below];

                int idx_below_shear = (g.fs_idx + l - 1) * g.NX + j;
                g.d_sxz_1[idx_above] = -g.d_sxz_1[idx_below_shear];
                g.d_sxz_2[idx_above] = -g.d_sxz_2[idx_below_shear];
            }
        }
    }
}

// --- Type 3 Free Surface Velocity Update ---
// Updates velocity fields for the free-surface solver and clears the air region.
__global__ void updateType3VelocityKernel(GPUSimData g) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= 4 && i < g.NZ - 4 && j >= 4 && j < g.NX - 4) {
        int k = i * g.NX + j;

        if (i < g.fs_idx) {
            g.d_vx_1[k] = 0.0f; g.d_vx_2[k] = 0.0f;
            g.d_vz_1[k] = 0.0f; g.d_vz_2[k] = 0.0f;
        }
        else {
            int row_dn1 = (i + 1) * g.NX; int row_dn2 = (i + 2) * g.NX;
            int row_dn3 = (i + 3) * g.NX; int row_dn4 = (i + 4) * g.NX;
            int row_up1 = (i - 1) * g.NX; int row_up2 = (i - 2) * g.NX; int row_up3 = (i - 3) * g.NX; int row_up4 = (i - 4) * g.NX;

            float d_z_val = (i <= g.fs_idx + 5 && g.upFlag) ? 0.0f : g.d_dz[i];
            float d_z_half_val = (i <= g.fs_idx + 5 && g.upFlag) ? 0.0f : g.d_dz_half[i];
            float d_x_val = g.d_dx[j];
            float d_x_half_val = g.d_dx_half[j];
            float cur_rho = g.d_rho[k];
            float inv_dt = g.inv_dt;

            float dsigxx_dx = g.c1_h * (g.d_sxx_1[k] + g.d_sxx_2[k] - g.d_sxx_1[k - 1] - g.d_sxx_2[k - 1])
                + g.c2_h * (g.d_sxx_1[k + 1] + g.d_sxx_2[k + 1] - g.d_sxx_1[k - 2] - g.d_sxx_2[k - 2])
                + g.c3_h * (g.d_sxx_1[k + 2] + g.d_sxx_2[k + 2] - g.d_sxx_1[k - 3] - g.d_sxx_2[k - 3])
                + g.c4_h * (g.d_sxx_1[k + 3] + g.d_sxx_2[k + 3] - g.d_sxx_1[k - 4] - g.d_sxx_2[k - 4]);

            float dsigxz_dz = g.c1_h * (g.d_sxz_1[k] + g.d_sxz_2[k] - g.d_sxz_1[row_up1 + j] - g.d_sxz_2[row_up1 + j])
                + g.c2_h * (g.d_sxz_1[row_dn1 + j] + g.d_sxz_2[row_dn1 + j] - g.d_sxz_1[row_up2 + j] - g.d_sxz_2[row_up2 + j])
                + g.c3_h * (g.d_sxz_1[row_dn1 + g.NX + j] + g.d_sxz_2[row_dn1 + g.NX + j] - g.d_sxz_1[row_up3 + j] - g.d_sxz_2[row_up3 + j])
                + g.c4_h * (g.d_sxz_1[row_dn1 + 2 * g.NX + j] + g.d_sxz_2[row_dn1 + 2 * g.NX + j] - g.d_sxz_1[row_up4 + j] - g.d_sxz_2[row_up4 + j]);

            g.d_vx_1[k] = (g.d_vx_1[k] * (inv_dt - d_x_val) + dsigxx_dx / cur_rho) / (inv_dt + d_x_val);
            g.d_vx_2[k] = (g.d_vx_2[k] * (inv_dt - d_z_val) + dsigxz_dz / cur_rho) / (inv_dt + d_z_val);

            float dsigxz_dx = g.c1_h * (g.d_sxz_1[k + 1] + g.d_sxz_2[k + 1] - g.d_sxz_1[k] - g.d_sxz_2[k])
                + g.c2_h * (g.d_sxz_1[k + 2] + g.d_sxz_2[k + 2] - g.d_sxz_1[k - 1] - g.d_sxz_2[k - 1])
                + g.c3_h * (g.d_sxz_1[k + 3] + g.d_sxz_2[k + 3] - g.d_sxz_1[k - 2] - g.d_sxz_2[k - 2])
                + g.c4_h * (g.d_sxz_1[k + 4] + g.d_sxz_2[k + 4] - g.d_sxz_1[k - 3] - g.d_sxz_2[k - 3]);

            float dsigzz_dz = g.c1_h * (g.d_szz_1[row_dn1 + j] + g.d_szz_2[row_dn1 + j] - g.d_szz_1[k] - g.d_szz_2[k])
                + g.c2_h * (g.d_szz_1[row_dn2 + j] + g.d_szz_2[row_dn2 + j] - g.d_szz_1[row_up1 + j] - g.d_szz_2[row_up1 + j])
                + g.c3_h * (g.d_szz_1[row_dn3 + j] + g.d_szz_2[row_dn3 + j] - g.d_szz_1[row_up2 + j] - g.d_szz_2[row_up2 + j])
                + g.c4_h * (g.d_szz_1[row_dn4 + j] + g.d_szz_2[row_dn4 + j] - g.d_szz_1[row_up3 + j] - g.d_szz_2[row_up3 + j]);

            float rho_avg = 0.25f * (g.d_rho[k] + g.d_rho[k + 1] + g.d_rho[row_dn1 + j] + g.d_rho[row_dn1 + j + 1]);
            g.d_vz_1[k] = (g.d_vz_1[k] * (inv_dt - d_x_half_val) + dsigxz_dx / rho_avg) / (inv_dt + d_x_half_val);
            g.d_vz_2[k] = (g.d_vz_2[k] * (inv_dt - d_z_half_val) + dsigzz_dz / rho_avg) / (inv_dt + d_z_half_val);
        }
    }
}

// --- Source Injection ---
// Injects one time-sampled source into the selected stress or velocity field.
__device__ void applyPointSource(GPUSimData g, float wavelet_val, int src_idx) {
    if (src_idx < 0 || src_idx >= g.total_grid || !isfinite(wavelet_val)) return;
    const int sourceX = src_idx % g.NX;
    const int sourceZ = src_idx / g.NX;
    if (sourceX < kStencilRadius || sourceX >= g.NX - kStencilRadius ||
        sourceZ < kStencilRadius || sourceZ >= g.NZ - kStencilRadius ||
        (g.flag_type == 3 && g.upFlag && sourceZ <= g.fs_idx)) return;
    // g.src_type:
    // 0: Vertical Force (Tzz) - 垂直方向集中力，直接作用于 Vz 分量
    // 1: Horizontal Force (Txx) - 水平方向集中力，直接作用于 Vx 分量
    // 2: Explosive (Txx + Tzz) - 各向同性膨胀压力源 (纯 P 波)，直接注入正应力张量
    // 3: Shear (Txz) - 纯剪切源 (生成强 S 波)，直接注入剪应力张量
    // 4: Rotated Force - 倾斜方向力源，根据 g.src_angle 分解并作用于 Vx 和 Vz

    if (g.src_type == 0) { // 垂直集中力
        float force_z = wavelet_val;
        if (g.flag_type == 1 || g.flag_type == 3) {
            float rho_avg = 0.25f * (g.d_rho[src_idx] + g.d_rho[src_idx + 1] + g.d_rho[src_idx + g.NX] + g.d_rho[src_idx + g.NX + 1]);
            atomicAdd(&g.d_vz_1[src_idx], force_z * g.dt / rho_avg);
        }
        else if (g.flag_type == 2) {
            atomicAdd(&g.d_vz[src_idx], force_z * g.dt / g.d_rho_x_z_flat[src_idx]);
        }
    }
    else if (g.src_type == 1) { // 水平集中力
        float force_x = wavelet_val;
        if (g.flag_type == 1 || g.flag_type == 3) {
            atomicAdd(&g.d_vx_1[src_idx], force_x * g.dt / g.d_rho[src_idx]);
        }
        else if (g.flag_type == 2) {
            atomicAdd(&g.d_vx[src_idx], force_x * g.dt / g.d_rho_orig_flat[src_idx]);
        }
    }
    else if (g.src_type == 2) { // 膨胀震源 (纯压缩 P 波，无横波分量) -> 作用于对角应力张量 [2]
        if (g.flag_type == 1 || g.flag_type == 3) {
            atomicAdd(&g.d_sxx_1[src_idx], wavelet_val * 0.5f);
            atomicAdd(&g.d_sxx_2[src_idx], wavelet_val * 0.5f);
            atomicAdd(&g.d_szz_1[src_idx], wavelet_val * 0.5f);
            atomicAdd(&g.d_szz_2[src_idx], wavelet_val * 0.5f);
        }
        else if (g.flag_type == 2) {
            atomicAdd(&g.d_sigmaxx[src_idx], wavelet_val);
            atomicAdd(&g.d_sigmazz[src_idx], wavelet_val);
        }
    }
    else if (g.src_type == 3) { // 剪切震源 (剪切波) -> 作用于离角剪切应力张量 Sxz
        if (g.flag_type == 1 || g.flag_type == 3) {
            atomicAdd(&g.d_sxz_1[src_idx], wavelet_val * 0.5f);
            atomicAdd(&g.d_sxz_2[src_idx], wavelet_val * 0.5f);
        }
        else if (g.flag_type == 2) {
            atomicAdd(&g.d_sigmaxz[src_idx], wavelet_val);
        }
    }
    else if (g.src_type == 4) { // 任意倾斜力源 (Rotated Force)
        float force_x = sinf(g.src_angle * M_PI / 180.0f) * wavelet_val;
        float force_z = cosf(g.src_angle * M_PI / 180.0f) * wavelet_val;
        if (g.flag_type == 1 || g.flag_type == 3) {
            atomicAdd(&g.d_vx_1[src_idx], force_x * g.dt / g.d_rho[src_idx]);
            float rho_avg = 0.25f * (g.d_rho[src_idx] + g.d_rho[src_idx + 1] + g.d_rho[src_idx + g.NX] + g.d_rho[src_idx + g.NX + 1]);
            atomicAdd(&g.d_vz_1[src_idx], force_z * g.dt / rho_avg);
        }
        else if (g.flag_type == 2) {
            atomicAdd(&g.d_vx[src_idx], force_x * g.dt / g.d_rho_orig_flat[src_idx]);
            atomicAdd(&g.d_vz[src_idx], force_z * g.dt / g.d_rho_x_z_flat[src_idx]);
        }
    }
}

// --- Zero Dirichlet Boundary ---
// 单点和多点震源共用同一种场注入方式，避免震源类型在两条路径中不一致。
__global__ void injectSourceKernel(GPUSimData g, float waveletValue, int sourceIndex) {
    applyPointSource(g, waveletValue, sourceIndex);
}

// Clears the outer stencil guard cells to prevent invalid edge reads.
__global__ void zeroBoundaryKernel(GPUSimData g) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < g.NX) {
        int top = idx;
        int btm = (g.NZ - 1) * g.NX + idx;
        if (g.flag_type == 1 || g.flag_type == 3) {
            g.d_vx_1[top] = 0; g.d_vx_2[top] = 0; g.d_vz_1[top] = 0; g.d_vz_2[top] = 0;
            g.d_vx_1[btm] = 0; g.d_vx_2[btm] = 0; g.d_vz_1[btm] = 0; g.d_vz_2[btm] = 0;
        }
        else {
            g.d_vx[top] = 0; g.d_vz[top] = 0; g.d_vx[btm] = 0; g.d_vz[btm] = 0;
        }
    }
    if (idx < g.NZ) {
        int left = idx * g.NX;
        int right = idx * g.NX + (g.NX - 1);
        if (g.flag_type == 1 || g.flag_type == 3) {
            g.d_vx_1[left] = 0; g.d_vx_2[left] = 0; g.d_vz_1[left] = 0; g.d_vz_2[left] = 0;
            g.d_vx_1[right] = 0; g.d_vx_2[right] = 0; g.d_vz_1[right] = 0; g.d_vz_2[right] = 0;
        }
        else {
            g.d_vx[left] = 0; g.d_vz[left] = 0; g.d_vx[right] = 0; g.d_vz[right] = 0;
        }
    }
}
// =============================================================================
// 【新增】：设备端安全越界钳位速度提取函数 (用于高阶有限差分求导)
// =============================================================================
// =============================================================================
// 【安全修复】：使用三目运算符彻底规避 windows.h 宏冲突与平台编译问题
// =============================================================================
__device__ float get_vx(const GPUSimData& g, int i, int j) {
    // 替代 max(0, min(i, g.NZ - 1))
    int r = (i < 0) ? 0 : ((i > g.NZ - 1) ? g.NZ - 1 : i);
    // 替代 max(0, min(j, g.NX - 1))
    int c = (j < 0) ? 0 : ((j > g.NX - 1) ? g.NX - 1 : j);

    int k = r * g.NX + c;
    return (g.flag_type == 1 || g.flag_type == 3) ? (g.d_vx_1[k] + g.d_vx_2[k]) : g.d_vx[k];
}

__device__ float get_vz(const GPUSimData& g, int i, int j) {
    // 替代 max(0, min(i, g.NZ - 1))
    int r = (i < 0) ? 0 : ((i > g.NZ - 1) ? g.NZ - 1 : i);
    // 替代 max(0, min(j, g.NX - 1))
    int c = (j < 0) ? 0 : ((j > g.NX - 1) ? g.NX - 1 : j);

    int k = r * g.NX + c;
    return (g.flag_type == 1 || g.flag_type == 3) ? (g.d_vz_1[k] + g.d_vz_2[k]) : g.d_vz[k];
}
// =============================================================================
// 【重构】：7 分量多重波场自适应实时色彩映射核函数 [2]
// =============================================================================
// =============================================================================
// 【自适应物理平衡】：7 分量多重波场自适应实时色彩映射核函数
// =============================================================================
// Converts a selected physical field component into an RGBA visualization buffer.
__global__ void generateColorMapKernel(GPUSimData g, uchar4* rgba_out, float scale, int component) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i < g.NZ && j < g.NX) {
        int k = i * g.NX + j;
        float val = 0.0f;
        float component_balancer = 1.0f; // 针对不同物理分量的自适应增益平衡因子

        if (component == 0) {
            // 1. Velocity Mag
            float vx = get_vx(g, i, j);
            float vz = get_vz(g, i, j);
            val = sqrtf(vx * vx + vz * vz);
            component_balancer = 1.0f;
        }
        else if (component == 1) {
            // 2. Velocity X
            val = get_vx(g, i, j);
            component_balancer = 1.0f;
        }
        else if (component == 2) {
            // 3. Velocity Y (Z)
            val = get_vz(g, i, j);
            component_balancer = 1.0f;
        }
        else if (component == 3) {
            // 4. Stress Normal (Sxx + Szz)
            float stress = 0.0f;
            if (g.flag_type == 1 || g.flag_type == 3) {
                stress = g.d_sxx_1[k] + g.d_sxx_2[k] + g.d_szz_1[k] + g.d_szz_2[k];
            }
            else if (g.flag_type == 2) {
                stress = g.d_sigmaxx[k] + g.d_sigmazz[k];
            }

            // 【物理归一化】：应力数值极大（GPa量级），除以本地纵波拉梅常数转换为应变尺度（10^-6 ~ 10^-3）
            float lam2mu = g.d_lambda2mu[k];
            val = (lam2mu > 1000.0f) ? (stress / lam2mu) : stress;
            val *= 1000;//处理后变小，需要继续增大1000左右
            component_balancer = 2.0f; // 归一化后的微调增益
        }
        else if (component == 4) {
            // 5. Stress Shear (Sxz)
            float shear = 0.0f;
            if (g.flag_type == 1 || g.flag_type == 3) {
                shear = g.d_sxz_1[k] + g.d_sxz_2[k];
            }
            else if (g.flag_type == 2) {
                shear = g.d_sigmaxz[k];
            }

            // 【物理归一化】：剪切应力除以剪切模量 mu
            float mu = g.d_mu[k];
            val = (mu > 1000.0f) ? (shear / mu) : shear;
            val *= 1000;
            component_balancer = 3.0f; // 归一化后的微调增益
        }
        else if (component == 5) {
            // 6. P-Wave (Div): 散度 (微商数值偏小，乘以 8.0 倍补偿) [2]
            float dvx_dx = get_vx(g, i, j) - get_vx(g, i, j - 1);
            float dvz_dz = get_vz(g, i, j) - get_vz(g, i - 1, j);
            val = dvx_dx + dvz_dz;
            component_balancer = 8.0f;
        }
        else if (component == 6) {
            // 7. S-Wave (Curl): 旋度 (微商数值偏小，乘以 8.0 倍补偿) [2]
            float dvz_dx = get_vz(g, i, j) - get_vz(g, i, j - 1);
            float dvx_dz = get_vx(g, i, j) - get_vx(g, i - 1, j);
            val = dvz_dx - dvx_dz;
            component_balancer = 8.0f;
        }

        // 应用自适应物理平衡因子，使所有通道在同一个 Color Gain 滑块下拥有高度一致的视觉密度
        float norm = val * scale * component_balancer;
        if (norm > 1.0f)  norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;

        unsigned char r = 255, g_val = 255, b = 255;
        if (norm > 0.0f) { // 正振幅 -> 红色
            g_val = static_cast<unsigned char>(255.0f * (1.0f - norm));
            b = static_cast<unsigned char>(255.0f * (1.0f - norm));
        }
        else { // 负振幅 -> 蓝色
            r = static_cast<unsigned char>(255.0f * (1.0f + norm));
            g_val = static_cast<unsigned char>(255.0f * (1.0f + norm));
        }

        rgba_out[k] = make_uchar4(r, g_val, b, 255);
    }
}

// Gathers Vx and Vz from flattened receiver indices for one simulation time step.
__global__ void extractReceiverSamplesKernel(
    const float* __restrict__ vx_1, const float* __restrict__ vx_2,
    const float* __restrict__ vz_1, const float* __restrict__ vz_2,
    const float* __restrict__ vx, const float* __restrict__ vz,
    const int* __restrict__ rcv_grid_idx,
    float* d_record_vx_step, float* d_record_vz_step,
    int num_rcv, int flag_type, int gridElementCount
) {
    int r = blockIdx.x * blockDim.x + threadIdx.x;
    if (r < num_rcv) {
        int k = rcv_grid_idx[r];
        if (k < 0 || k >= gridElementCount) {
            d_record_vx_step[r] = 0.0f;
            d_record_vz_step[r] = 0.0f;
            return;
        }
        if (flag_type == 1 || flag_type == 3) {
            d_record_vx_step[r] = vx_1[k] + vx_2[k];
            d_record_vz_step[r] = vz_1[k] + vz_2[k];
        }
        else {
            d_record_vx_step[r] = vx[k];
            d_record_vz_step[r] = vz[k];
        }
    }
}

// --- Multi-source Injection Kernel ---
// Injects all active point sources from the device-side source array.
__global__ void injectMultipleSourcesKernel(GPUSimData g, const GPUSource* d_sources, int num_sources) {
    int r = threadIdx.x;
    if (r < num_sources) {
        GPUSource src = d_sources[r];
        if (!isfinite(src.f_peak) || src.f_peak <= 0.0f || !isfinite(src.t)) return;

        float t_peak = 1.0f / src.f_peak;
        float x = M_PI * src.f_peak * (src.t - t_peak);
        float val = src.amp * (1.0f - 2.0f * x * x) * expf(-x * x);

        applyPointSource(g, val * kSourceInjectionScale2D, src.idx);
    }
}

// =============================================================================
// 主机驱动接口实现 (Host Drivers)
// =============================================================================

// 在任何 CUDA 分配前拒绝非法尺寸、数组长度和非物理材料参数。
void validateSimulationContext(const SimulationContext& ctx) {
    const size_t elementCount = checkedGridElementCount(ctx.NX, ctx.NZ);
    validateBoundaryGeometry(ctx.NX, ctx.NZ, ctx.npml, ctx.flag_type == 3 && ctx.upFlag);
    if (ctx.flag_type < 1 || ctx.flag_type > 3 || ctx.total_grid != elementCount ||
        !std::isfinite(ctx.dt) || ctx.dt <= 0.0f || !std::isfinite(ctx.h) || ctx.h <= 0.0f || ctx.nt <= 0) {
        throw std::invalid_argument("Invalid solver, time grid, or spatial grid");
    }
    if (ctx.rho.size() != elementCount || ctx.mu.size() != elementCount ||
        ctx.lambda.size() != elementCount || ctx.lambda2mu.size() != elementCount ||
        ctx.dx.size() != ctx.NX || ctx.dx_half.size() != ctx.NX ||
        ctx.dz.size() != ctx.NZ || ctx.dz_half.size() != ctx.NZ ||
        (ctx.flag_type == 3 && ctx.dp_flat.size() != ctx.NX) ||
        ctx.wavelet.size() < static_cast<size_t>(ctx.nt) || ctx.num_rcv < 0 ||
        ctx.rcv_grid_idx.size() != static_cast<size_t>(ctx.num_rcv)) {
        throw std::invalid_argument("Simulation array lengths do not match the grid or receiver count");
    }
    float maximumVp = 0.0f;
    for (size_t k = 0; k < elementCount; ++k) {
        if (!std::isfinite(ctx.rho[k]) || ctx.rho[k] <= 0.0f ||
            !std::isfinite(ctx.mu[k]) || ctx.mu[k] < 0.0f ||
            !std::isfinite(ctx.lambda[k]) || !std::isfinite(ctx.lambda2mu[k]) || ctx.lambda2mu[k] <= 0.0f) {
            throw std::invalid_argument("Density/stiffness must be finite with rho > 0, mu >= 0, lambda2mu > 0");
        }
        maximumVp = std::max(maximumVp, std::sqrt(ctx.lambda2mu[k] / ctx.rho[k]));
        if (ctx.lambda2mu[k] - (4.0f / 3.0f) * ctx.mu[k] <= 0.0f) {
            throw std::invalid_argument("Elastic bulk modulus must be positive");
        }
    }
    if (!std::isfinite(maximumVp) || ctx.dt * maximumVp / ctx.h > kCourantLimit2d) {
        throw std::invalid_argument("The time step exceeds the 2D staggered-grid CFL limit");
    }
    for (const auto* profile : {&ctx.dx, &ctx.dx_half, &ctx.dz, &ctx.dz_half}) {
        for (float damping : *profile) {
            if (!std::isfinite(damping) || damping < 0.0f) {
                throw std::invalid_argument("PML damping must be finite and nonnegative");
            }
        }
    }
    for (int receiverIndex : ctx.rcv_grid_idx) {
        if (receiverIndex < 0 || receiverIndex >= ctx.total_grid) {
            throw std::invalid_argument("A receiver lies outside the simulation grid");
        }
    }
}

// Allocates device buffers; failure releases every allocation made by this attempt.
void initializeGpuSimulation(GPUSimData& gpu, const SimulationContext& ctx) {
    validateSimulationContext(ctx);
    releaseGpuSimulation(gpu);
    try {
    gpu.NX = ctx.NX; gpu.NZ = ctx.NZ; gpu.total_grid = ctx.total_grid;
    gpu.dt = ctx.dt; gpu.inv_dt = 1.0f / ctx.dt;
    gpu.flag_type = ctx.flag_type; gpu.npml = ctx.npml;
    gpu.fs_idx = ctx.npml; gpu.upFlag = ctx.upFlag;
    gpu.c1_h = ctx.c1_h; gpu.c2_h = ctx.c2_h; gpu.c3_h = ctx.c3_h; gpu.c4_h = ctx.c4_h;

    size_t bytes = ctx.total_grid * sizeof(float);

    CUDA_CHECK(cudaMalloc(&gpu.d_vx_1, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vx_1, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_vx_2, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vx_2, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_vz_1, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vz_1, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_vz_2, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vz_2, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_sxx_1, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sxx_1, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_sxx_2, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sxx_2, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_szz_1, bytes)); CUDA_CHECK(cudaMemset(gpu.d_szz_1, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_szz_2, bytes)); CUDA_CHECK(cudaMemset(gpu.d_szz_2, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_sxz_1, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sxz_1, 0, bytes));
    CUDA_CHECK(cudaMalloc(&gpu.d_sxz_2, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sxz_2, 0, bytes));

    CUDA_CHECK(cudaMalloc(&gpu.d_rho, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_rho, ctx.rho.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_mu, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_mu, ctx.mu.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_lambda, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_lambda, ctx.lambda.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_lambda2mu, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_lambda2mu, ctx.lambda2mu.data(), bytes, cudaMemcpyHostToDevice));

    CUDA_CHECK(cudaMalloc(&gpu.d_dx, ctx.NX * sizeof(float))); CUDA_CHECK(cudaMemcpy(gpu.d_dx, ctx.dx.data(), ctx.NX * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_dx_half, ctx.NX * sizeof(float))); CUDA_CHECK(cudaMemcpy(gpu.d_dx_half, ctx.dx_half.data(), ctx.NX * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_dz, ctx.NZ * sizeof(float))); CUDA_CHECK(cudaMemcpy(gpu.d_dz, ctx.dz.data(), ctx.NZ * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMalloc(&gpu.d_dz_half, ctx.NZ * sizeof(float))); CUDA_CHECK(cudaMemcpy(gpu.d_dz_half, ctx.dz_half.data(), ctx.NZ * sizeof(float), cudaMemcpyHostToDevice));

    CUDA_CHECK(cudaMalloc(&gpu.d_active_sources, kMaximumActiveSources * sizeof(GPUSource)));

    if (gpu.flag_type == 2) {
        CUDA_CHECK(cudaMalloc(&gpu.d_vx, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vx, 0, bytes));
        CUDA_CHECK(cudaMalloc(&gpu.d_vz, bytes)); CUDA_CHECK(cudaMemset(gpu.d_vz, 0, bytes));
        CUDA_CHECK(cudaMalloc(&gpu.d_sigmaxx, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sigmaxx, 0, bytes));
        CUDA_CHECK(cudaMalloc(&gpu.d_sigmazz, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sigmazz, 0, bytes));
        CUDA_CHECK(cudaMalloc(&gpu.d_sigmaxz, bytes)); CUDA_CHECK(cudaMemset(gpu.d_sigmaxz, 0, bytes));

        std::vector<float> mu_x(ctx.total_grid, 0.0f), lambda_x(ctx.total_grid, 0.0f), l2m_x(ctx.total_grid, 0.0f);
        std::vector<float> mu_z(ctx.total_grid, 0.0f), rho_xz(ctx.total_grid, 0.0f), rho_orig(ctx.total_grid, 0.0f);
        for (int i = 0; i < ctx.NZ; ++i) {
            for (int j = 0; j < ctx.NX; ++j) {
                int k = i * ctx.NX + j;
                rho_orig[k] = ctx.rho[k];
                int k_next_x = (j < ctx.NX - 1) ? k + 1 : k;
                int k_next_z = (i < ctx.NZ - 1) ? k + ctx.NX : k;
                mu_x[k] = 0.5f * (ctx.mu[k] + ctx.mu[k_next_x]);
                lambda_x[k] = 0.5f * (ctx.lambda[k] + ctx.lambda[k_next_x]);
                l2m_x[k] = 0.5f * (ctx.lambda2mu[k] + ctx.lambda2mu[k_next_x]);
                mu_z[k] = 0.5f * (ctx.mu[k] + ctx.mu[k_next_z]);
                int k_dr = (i < ctx.NZ - 1 && j < ctx.NX - 1) ? k + ctx.NX + 1 : k;
                rho_xz[k] = 0.25f * (ctx.rho[k] + ctx.rho[k_next_x] + ctx.rho[k_next_z] + ctx.rho[k_dr]);
            }
        }
        CUDA_CHECK(cudaMalloc(&gpu.d_mu_x_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_mu_x_flat, mu_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&gpu.d_lambda_x_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_lambda_x_flat, lambda_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&gpu.d_lambda2mu_x_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_lambda2mu_x_flat, l2m_x.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&gpu.d_mu_z_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_mu_z_flat, mu_z.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&gpu.d_rho_x_z_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_rho_x_z_flat, rho_xz.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMalloc(&gpu.d_rho_orig_flat, bytes)); CUDA_CHECK(cudaMemcpy(gpu.d_rho_orig_flat, rho_orig.data(), bytes, cudaMemcpyHostToDevice));
    }

    if (gpu.flag_type == 3) {
        CUDA_CHECK(cudaMalloc(&gpu.d_dp_flat, ctx.NX * sizeof(float)));
        CUDA_CHECK(cudaMemcpy(gpu.d_dp_flat, ctx.dp_flat.data(), ctx.NX * sizeof(float), cudaMemcpyHostToDevice));
    }

    CUDA_CHECK(cudaMalloc(&gpu.d_wavelet, ctx.wavelet.size() * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(gpu.d_wavelet, ctx.wavelet.data(), ctx.wavelet.size() * sizeof(float), cudaMemcpyHostToDevice));

    updateGpuReceivers(gpu, ctx.rcv_grid_idx);
    }
    catch (...) {
        releaseGpuSimulation(gpu);
        throw;
    }
}

// Releases device buffers owned by the selected solver scheme.
void releaseGpuSimulation(GPUSimData& gpu) {
    cudaFree(gpu.d_vx_1); cudaFree(gpu.d_vx_2); cudaFree(gpu.d_vz_1); cudaFree(gpu.d_vz_2);
    cudaFree(gpu.d_sxx_1); cudaFree(gpu.d_sxx_2); cudaFree(gpu.d_szz_1); cudaFree(gpu.d_szz_2);
    cudaFree(gpu.d_sxz_1); cudaFree(gpu.d_sxz_2);
    cudaFree(gpu.d_rho); cudaFree(gpu.d_mu); cudaFree(gpu.d_lambda); cudaFree(gpu.d_lambda2mu);
    cudaFree(gpu.d_dx); cudaFree(gpu.d_dx_half); cudaFree(gpu.d_dz); cudaFree(gpu.d_dz_half);
    cudaFree(gpu.d_wavelet); cudaFree(gpu.d_rcv_grid_idx);
    cudaFree(gpu.d_active_sources);

    // 释放检波器单步记录临时缓冲区
    cudaFree(gpu.d_record_vx_step);
    cudaFree(gpu.d_record_vz_step);

        cudaFree(gpu.d_vx); cudaFree(gpu.d_vz);
        cudaFree(gpu.d_sigmaxx); cudaFree(gpu.d_sigmazz); cudaFree(gpu.d_sigmaxz);
        cudaFree(gpu.d_mu_x_flat); cudaFree(gpu.d_lambda_x_flat); cudaFree(gpu.d_lambda2mu_x_flat);
        cudaFree(gpu.d_mu_z_flat); cudaFree(gpu.d_rho_x_z_flat); cudaFree(gpu.d_rho_orig_flat);
        cudaFree(gpu.d_dp_flat);
    gpu = GPUSimData{};
}

// 先准备新的检波器缓冲区，全部成功后才替换旧资源；空数组会清除旧采集布局。
void updateGpuReceivers(GPUSimData& gpu, const std::vector<int>& receiverIndices) {
    if (receiverIndices.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        throw std::invalid_argument("Too many receivers");
    }
    for (int index : receiverIndices) {
        if (index < 0 || index >= gpu.total_grid) {
            throw std::invalid_argument("Receiver index is outside the active GPU grid");
        }
    }
    int* newIndices = nullptr;
    float* newVx = nullptr;
    float* newVz = nullptr;
    try {
        if (!receiverIndices.empty()) {
            const size_t sampleBytes = receiverIndices.size() * sizeof(float);
            CUDA_CHECK(cudaMalloc(&newIndices, receiverIndices.size() * sizeof(int)));
            CUDA_CHECK(cudaMemcpy(newIndices, receiverIndices.data(), receiverIndices.size() * sizeof(int), cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaMalloc(&newVx, sampleBytes));
            CUDA_CHECK(cudaMalloc(&newVz, sampleBytes));
            CUDA_CHECK(cudaMemset(newVx, 0, sampleBytes));
            CUDA_CHECK(cudaMemset(newVz, 0, sampleBytes));
        }
    }
    catch (...) {
        cudaFree(newIndices); cudaFree(newVx); cudaFree(newVz);
        throw;
    }
    cudaFree(gpu.d_rcv_grid_idx); cudaFree(gpu.d_record_vx_step); cudaFree(gpu.d_record_vz_step);
    gpu.d_rcv_grid_idx = newIndices;
    gpu.d_record_vx_step = newVx;
    gpu.d_record_vz_step = newVz;
    gpu.receiverCapacity = static_cast<int>(receiverIndices.size());
}

// Launches stress, velocity, source-injection, and boundary kernels for one time step.
void runGpuSimulationStep(GPUSimData& gpu, int current_it, const SimulationContext& ctx, int num_active_sources) {
    if (gpu.total_grid <= 0 || current_it < 0 || num_active_sources > kMaximumActiveSources) {
        throw std::invalid_argument("Invalid GPU simulation state, iteration, or source count");
    }
    dim3 block(16, 16);
    dim3 grid((gpu.NX + block.x - 1) / block.x, (gpu.NZ + block.y - 1) / block.y);

    // 同步主机端控制面板的震源类型及角度到 GPU 结构体
    gpu.src_type = ctx.src_type;
    gpu.src_angle = ctx.src_angle;

    if (gpu.flag_type == 1 || (gpu.flag_type == 3 && !gpu.upFlag)) {
        updateType1StressKernel << <grid, block >> > (gpu);
    }
    else if (gpu.flag_type == 2) {
        updateType2StressKernel << <grid, block >> > (gpu);
    }
    else if (gpu.flag_type == 3) {
        updateType3StressKernel << <grid, block >> > (gpu);
        if (gpu.upFlag) {
            mirrorType3StressKernel << <(gpu.NX + 255) / 256, 256 >> > (gpu);
        }
    }
    CUDA_CHECK(cudaGetLastError());

    if (gpu.flag_type == 1 || (gpu.flag_type == 3 && !gpu.upFlag)) {
        updateType1VelocityKernel << <grid, block >> > (gpu);
    }
    else if (gpu.flag_type == 2) {
        updateType2VelocityKernel << <grid, block >> > (gpu);
    }
    else if (gpu.flag_type == 3) {
        updateType3VelocityKernel << <grid, block >> > (gpu);
    }
    CUDA_CHECK(cudaGetLastError());

    // 3. Source Injection
    if (num_active_sources >= 0) {
        if (num_active_sources > 0) {
            injectMultipleSourcesKernel << <1, num_active_sources >> > (gpu, gpu.d_active_sources, num_active_sources);
        }
    }
    else {
        if (current_it < ctx.nt && current_it < static_cast<int>(ctx.wavelet.size())) {
            // 在注入时，直接将基础子波值乘以幅值增益系数 ctx.src_amp，实现零时延实时调控
            float wavelet_val = ctx.wavelet[current_it] * ctx.src_amp * kSourceInjectionScale2D;

            int safe_src_z = (gpu.flag_type == 3 && gpu.upFlag) ? std::max(ctx.src_z_idx, gpu.fs_idx + 1) : ctx.src_z_idx;
            int src_idx = safe_src_z * gpu.NX + (ctx.src_idx % gpu.NX);

            injectSourceKernel << <1, 1 >> > (gpu, wavelet_val, src_idx);
        }
    }
    CUDA_CHECK(cudaGetLastError());

    int max_dim = std::max(gpu.NX, gpu.NZ);
    zeroBoundaryKernel << <(max_dim + 255) / 256, 256 >> > (gpu);
    CUDA_CHECK(cudaGetLastError());
}

// Copies the vertical-velocity component to host memory, recombining split fields when required.
void copyWavefieldToHost(GPUSimData& gpu, float* h_vz_out) {
    if (!h_vz_out || gpu.total_grid <= 0) throw std::invalid_argument("Invalid wavefield output buffer");
    size_t bytes = gpu.total_grid * sizeof(float);
    if (gpu.flag_type == 1 || gpu.flag_type == 3) {
        std::vector<float> temp_v1(gpu.total_grid);
        std::vector<float> temp_v2(gpu.total_grid);
        CUDA_CHECK(cudaMemcpy(temp_v1.data(), gpu.d_vz_1, bytes, cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(temp_v2.data(), gpu.d_vz_2, bytes, cudaMemcpyDeviceToHost));
        for (int i = 0; i < gpu.total_grid; i++) {
            h_vz_out[i] = temp_v1[i] + temp_v2[i];
        }
    }
    else {
        CUDA_CHECK(cudaMemcpy(h_vz_out, gpu.d_vz, bytes, cudaMemcpyDeviceToHost));
    }
}

// Extracts receiver values for a specified time index into host trace arrays.
void copyReceiverRecordsFromGpu(GPUSimData& gpu, int num_rcv, int nt, int current_it, float* h_rec_vx, float* h_rec_vz) {
    if (num_rcv <= 0) return;
    if (num_rcv > gpu.receiverCapacity || nt <= 0 || current_it < 0 || current_it >= nt || !h_rec_vx || !h_rec_vz) {
        throw std::invalid_argument("Invalid receiver trace count, time index, or host output buffer");
    }

    int threads = 128;
    int blocks = (num_rcv + threads - 1) / threads;
    extractReceiverSamplesKernel << <blocks, threads >> > (
        gpu.d_vx_1, gpu.d_vx_2, gpu.d_vz_1, gpu.d_vz_2,
        gpu.d_vx, gpu.d_vz,
        gpu.d_rcv_grid_idx,
        gpu.d_record_vx_step,
        gpu.d_record_vz_step,
        num_rcv, gpu.flag_type, gpu.total_grid
        );

    std::vector<float> h_step_vx(num_rcv);
    std::vector<float> h_step_vz(num_rcv);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaMemcpy(h_step_vx.data(), gpu.d_record_vx_step, num_rcv * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(h_step_vz.data(), gpu.d_record_vz_step, num_rcv * sizeof(float), cudaMemcpyDeviceToHost));

    for (int r = 0; r < num_rcv; ++r) {
        h_rec_vx[static_cast<size_t>(r) * nt + current_it] = h_step_vx[r];
        h_rec_vz[static_cast<size_t>(r) * nt + current_it] = h_step_vz[r];
    }
}

// Converts the requested wavefield component to RGBA pixels for CUDA-OpenGL display.
void generateWavefieldTextureOnGpu(GPUSimData& gpu, uchar4* d_rgba_out, float color_scale, int show_component) {
    dim3 block(16, 16);
    dim3 grid((gpu.NX + block.x - 1) / block.x, (gpu.NZ + block.y - 1) / block.y);
    generateColorMapKernel << <grid, block >> > (gpu, d_rgba_out, color_scale, show_component);
    CUDA_CHECK(cudaGetLastError());
}

// =============================================================================
// 【新增】： recordReceiverStepOnGpu 的主机端包装实现 (MSVC 与 NVCC 之间的安全桥梁)
// =============================================================================
// Extracts one receiver sample per channel and synchronously copies Vx and Vz arrays to host.
void recordReceiverStepOnGpu(GPUSimData& gpu, int num_rcv, float* h_out_vx, float* h_out_vz) {
    if (num_rcv <= 0) return;
    if (num_rcv > gpu.receiverCapacity || !h_out_vx || !h_out_vz) {
        throw std::invalid_argument("Invalid receiver count or host output buffer");
    }

    // 1. 在 GPU 端高并发提取当前时步下所有接收器的振幅
    int threads = 128;
    int blocks = (num_rcv + threads - 1) / threads;
    extractReceiverSamplesKernel << <blocks, threads >> > (
        gpu.d_vx_1, gpu.d_vx_2, gpu.d_vz_1, gpu.d_vz_2,
        gpu.d_vx, gpu.d_vz,
        gpu.d_rcv_grid_idx,
        gpu.d_record_vx_step,
        gpu.d_record_vz_step,
        num_rcv, gpu.flag_type, gpu.total_grid
        );

    // 2. 将 GPU 提取出的单步振幅拷回由 C++ 传入的 Host 临时数组中 (瞬间完成，无任何总线拥堵)
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaMemcpy(h_out_vx, gpu.d_record_vx_step, num_rcv * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(h_out_vz, gpu.d_record_vz_step, num_rcv * sizeof(float), cudaMemcpyDeviceToHost));
}

// =============================================================================
// 【解决循环依赖】：实现 Common.h 中声明的 ReplayManager::uploadFrameToGpu 成员函数
// =============================================================================
// Restores a cached replay frame into the active solver's vertical-velocity fields.
void ReplayManager::uploadFrameToGpu(int frameIdx, GPUSimData& gpu) {
    if (frameIdx < 0 || frameIdx >= static_cast<int>(frames.size())) return;
    if (frames[frameIdx].size() != static_cast<size_t>(gpu.total_grid)) {
        throw std::invalid_argument("Replay frame dimensions do not match the active model");
    }
    size_t bytes = gpu.total_grid * sizeof(float);

    if (gpu.flag_type == 1 || gpu.flag_type == 3) {
        // 覆盖 vz_1，并将 vz_2 抹零，使其和正好等于当前帧数据
        CUDA_CHECK(cudaMemcpy(gpu.d_vz_1, frames[frameIdx].data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemset(gpu.d_vz_2, 0, bytes));
    }
    else if (gpu.flag_type == 2) {
        CUDA_CHECK(cudaMemcpy(gpu.d_vz, frames[frameIdx].data(), bytes, cudaMemcpyHostToDevice));
    }
}

