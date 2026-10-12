#pragma once
#include "Seis3D_Sim.h"
#include "Cuda_Seis3D.cuh"
#include <stdexcept>
namespace Seis3D::Detail3D {
inline constexpr double gibibyte3D=1024.0*1024.0*1024.0;
// Decode an editable UTF-8 filesystem path.
std::filesystem::path uiPath3D(const char* text);
// Convert CUDA failures into a sandbox-local exception.
void requireCuda3D(cudaError_t result,const char* operation);
// Create a unique acquisition directory suffix.
std::string sessionSuffix3D();
// Allocate private rendering resources after the physical model is ready.
void initializeGraphics3D(Seis3D_State& s);
// Refresh scalar data as needed and render the current transfer functions.
void updateGraphics3D(Seis3D_State& s);
// Replace the simulation using validated procedural or imported material data.
void initializeSession3D(Seis3D_State& s,bool imported);
// Inspect all imported cells before replacing live GPU resources.
SimulationContext3D inspectModel3D(const std::filesystem::path& path,SimulationContext3D context);
// Stream a validated model into the newly allocated GPU session.
void importModel3D(Seis3D_State& s,const std::filesystem::path& path);
// Stream the current model to a new output file.
void exportModel3D(Seis3D_State& s);
// Fit receiver and source spreads to the current model dimensions.
void fitSurvey3D(Seis3D_State& s);
// Validate and upload the selected receiver geometry.
void configureReceivers3D(Seis3D_State& s);
// Freeze geometry and start a new shot gather.
void startGather3D(Seis3D_State& s,int shotId);
// Export the frozen three-component gather and geometry.
void exportGather3D(Seis3D_State& s,const std::filesystem::path& directory);
// Select the next validated source in the shot spread.
void positionSequenceShot3D(Seis3D_State& s);
// Advance a bounded simulation batch and sample receivers at the requested cadence.
void advanceSimulation3D(Seis3D_State& s);
// Keep errors from UI operations local and retain captured data for retrying a failed export.
template<class Action>
void performAction3D(Seis3D_State& s, Action action) {
    const auto previousMessage=s.message3D;
    try { action(); if(s.message3D!=previousMessage) s.messageIsError3D=false; }
    catch (const std::exception& e) {
        s.running3D=false; s.sequence3D=false; s.recording3D=false;
        s.message3D=e.what(); s.messageIsError3D=true;
    }
}


} // namespace Seis3D::Detail3D
