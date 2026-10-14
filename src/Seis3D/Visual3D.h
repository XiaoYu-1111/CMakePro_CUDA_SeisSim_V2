#pragma once
#include "imgui.h"
#include "WavePalettes.h"
#include <algorithm>
#include <array>

namespace Seis3D::Detail3D {
inline constexpr const char* waveStyleNames3D=Seismic::wavePaletteNames;
inline constexpr int waveStyleCount3D=14, waveComponentCount3D=5, modelStyleCount3D=5;
inline constexpr const char* modelStyleNames3D="Dark Titanium\0Geological Map\0Grayscale Velocity\0WaveStyle Synced\0Scientific White\0";
using Palette3D=Seismic::WavePalette;
// The wavefield and record viewers use one stateless palette definition.
inline const Palette3D& wavePalette3D(int style) {return Seismic::wavePalette(style);}
// Interpolate the GPU palette for an accurate, compact on-screen legend.
inline ImU32 paletteColor3D(int style,float t) {
    const auto& p=wavePalette3D(style);
    const float q=(std::clamp)(t,0.0f,1.0f)*4;
    const int i=(std::min)(int(q),3); const float f=q-i;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(p[i][0]+(p[i+1][0]-p[i][0])*f,
        p[i][1]+(p[i+1][1]-p[i][1])*f,p[i][2]+(p[i+1][2]-p[i][2])*f,1));
}
} // namespace Seis3D::Detail3D
