#pragma once
#include "Common3D.h"
#include <GL/glew.h>
#include <cuda_gl_interop.h>
#include <filesystem>
#include <string>
#include <vector>

namespace Seis3D {

struct GLHandles3D {
    GLuint sliceProgram3D = 0, volumeProgram3D = 0, vao3D = 0;
    GLuint stagingBuffer3D = 0, volumeTexture3D = 0;
    GLuint volumeFramebuffer3D = 0, volumeDisplayTexture3D = 0;
    GLuint scalarTextures3D[3]{}, displayTextures3D[3]{}, framebuffers3D[3]{};
    cudaGraphicsResource_t interop3D = nullptr;
    ViewLayout3D layout3D;
    std::array<float,32> renderedSettings3D{};
    bool rendered3D = false;
};

struct VolumeCamera3D {
    float yaw3D = 0.65f, pitch3D = 0.35f, distance3D = 1.65f;
    float pan3D[2]{};
};

struct Survey3D {
    int receiverCountX3D = 8, receiverCountY3D = 8;
    int receiverOrigin3D[3] = { 8, 8, 0 };
    int receiverSpacing3D[2] = { 10, 10 };
    int shotCountX3D = 3, shotCountY3D = 2;
    int shotOrigin3D[3] = { 24, 24, 12 };
    int shotSpacing3D[2] = { 20, 20 };
    double worldOrigin3D[2] = { 0.0, 0.0 };
    float cmpBinSize3D[2] = { 25.0f, 25.0f };
};

struct Gather3D {
    SimulationContext3D context3D;
    Source3D source3D;
    Survey3D survey3D;
    std::vector<Receiver3D> receivers3D;
    std::vector<float> samples3D; // [sample][receiver][vx,vy,vz].
    int sampleCount3D = 0, targetSamples3D = 512, stride3D = 2, shotId3D = 1;
    int sampleIntervalUs3D = 0;
    int coordinateScalar3D = -1000;
    double firstSampleTime3D = 0.0;
};

struct Seis3D_State {
    SimulationContext3D context3D, editContext3D;
    GPUSimData3D gpu3D;
    GLHandles3D gl3D;
    Source3D source3D;
    Survey3D survey3D;
    Gather3D gather3D;
    VolumeCamera3D camera3D;
    std::vector<Receiver3D> receivers3D;
    bool ready3D = false, running3D = false, dirty3D = true;
    bool showVolume3D = true, autoScale3D = true, recording3D = false, sequence3D = false;
    bool homeRequested3D = false, singleStep3D = false;
    bool resetLayout3D = false, showGrid3D = true, showCrosshair3D = true;
    bool panelsOpen3D[7] = {true,true,true,true,true,true,true};
    int layoutPreset3D = 0, displayMode3D = 0, waveStyle3D = 13, modelStyle3D = 1;
    int materialComponent3D = 5, displayStride3D = 1;
    std::uint64_t displayedStep3D = 0;
    bool materialRangeAuto3D = true;
    float materialRange3D[2] = {0,1}, manualMaterialRange3D[2] = {1500,5000};
    float waveOpacity3D = 0.9f, waveThreshold3D = 0.025f;
    float modelOpacity3D = 0.3f, gridOpacity3D = 0.18f;
    int component3D = 0, stepsPerFrame3D = 2, slice3D[3] = {48,48,24};
    int targetSamples3D = 512, samplingStride3D = 2, sequenceIndex3D = 0;
    int maximumSteps3D = 2000, selectedReceiver3D = 0;
    std::uint64_t step3D = 0;
    float viewMaximum3D = 1.0f, manualScale3D = 0.01f, gain3D = 1.0f;
    float volumeOpacity3D = 1.5f, volumeThreshold3D = 0.06f, computeMs3D = 0.0f;
    double memoryRefreshTime3D = -1.0;
    std::size_t freeVram3D = 0, totalVram3D = 0;
    Receiver3D hoverPoint3D{-1,-1,-1};
    float hoverMaterial3D[3]{};
    bool hoverValid3D = false;
    char modelPath3D[1024]{};
    char exportDirectory3D[1024]{};
    std::filesystem::path sequenceDirectory3D;
    std::string message3D = "Choose a model, then Initialize 3D. GPU resources are allocated on demand.";
    bool messageIsError3D = false;

    // Initialize only output paths; constructing the state must not allocate GPU resources.
    Seis3D_State();
    // Destroy this sandbox before the application destroys its current GL context.
    ~Seis3D_State();
    Seis3D_State(const Seis3D_State&) = delete;
    Seis3D_State& operator=(const Seis3D_State&) = delete;
};

// Draw the complete independent workspace; return true when Home/Tab requests a cold route change.
bool renderScreen3D(Seis3D_State& state3D, int width3D, int height3D);
// Draw the independent launch card; return 1 for the sandbox, 2 for device details, otherwise 0.
int drawIntroCard3D(float x3D, float y3D, float width3D, float height3D, float scale3D,
    const char* deviceName3D, double vramGiB3D);
// Release CUDA/GL resources in the correct order; no global device reset and no 2-D state access.
void releaseSandbox3D(Seis3D_State& state3D) noexcept;

} // namespace Seis3D
