#pragma once
#include "imgui.h"
#include <algorithm>
#include <array>

namespace Seis3D::Detail3D {
inline constexpr const char* waveStyleNames3D = "Seismic Bipolar\0Coolwarm\0PuOr\0Grayscale\0Inferno\0Plasma\0Viridis\0Turbo\0Magma Glow\0Jet Rainbow\0Hot\0Cold\0Terrain Map\0CUDA Green\0";
using Palette3D = std::array<std::array<float,3>,5>;
// Share the same five-stop display palettes between GPU transfer functions and UI legends.
inline const Palette3D& wavePalette3D(int style) {
    static constexpr Palette3D palettes[] = {
        {{{.05f,.15f,.65f},{.25f,.6f,.95f},{.96f,.97f,.98f},{.96f,.5f,.35f},{.65f,.03f,.08f}}},
        {{{.23f,.3f,.75f},{.55f,.69f,.99f},{.86f,.86f,.86f},{.96f,.6f,.48f},{.71f,.02f,.15f}}},
        {{{.18f,.05f,.35f},{.6f,.5f,.73f},{.97f,.96f,.93f},{.96f,.65f,.3f},{.5f,.23f,.03f}}},
        {{{0,0,0},{.25f,.25f,.25f},{.5f,.5f,.5f},{.75f,.75f,.75f},{1,1,1}}},
        {{{.001f,0,.014f},{.34f,.06f,.43f},{.73f,.21f,.33f},{.98f,.56f,.04f},{.99f,1,.65f}}},
        {{{.05f,.03f,.53f},{.5f,.02f,.66f},{.8f,.28f,.47f},{.97f,.58f,.25f},{.94f,.98f,.13f}}},
        {{{.27f,.005f,.33f},{.23f,.32f,.55f},{.13f,.57f,.55f},{.37f,.79f,.38f},{.99f,.91f,.14f}}},
        {{{.19f,.07f,.23f},{.16f,.73f,.93f},{.64f,.99f,.23f},{.98f,.5f,.13f},{.48f,.02f,.01f}}},
        {{{.001f,0,.014f},{.32f,.07f,.48f},{.72f,.21f,.48f},{.99f,.53f,.38f},{.99f,.99f,.75f}}},
        {{{0,0,.5f},{0,.5f,1},{.5f,1,.5f},{1,.5f,0},{.5f,0,0}}},
        {{{0,0,0},{.65f,0,0},{1,.25f,0},{1,.9f,0},{1,1,1}}},
        {{{0,0,0},{0,.12f,.5f},{0,.4f,.85f},{0,.9f,1},{.85f,1,1}}},
        {{{.12f,.25f,.35f},{.22f,.52f,.38f},{.65f,.66f,.39f},{.65f,.46f,.3f},{.96f,.93f,.84f}}},
        {{{.1f,.48f,.98f},{.08f,.24f,.39f},{.028f,.045f,.052f},{.24f,.5f,.12f},{.48f,.94f,.2f}}}
    };
    return palettes[(std::clamp)(style,0,13)];
}
// Interpolate the GPU palette for an accurate, compact on-screen legend.
inline ImU32 paletteColor3D(int style,float t) {
    const auto& p=wavePalette3D(style);
    const float q=(std::clamp)(t,0.0f,1.0f)*4;
    const int i=(std::min)(int(q),3); const float f=q-i;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(p[i][0]+(p[i+1][0]-p[i][0])*f,
        p[i][1]+(p[i+1][1]-p[i][1])*f,p[i][2]+(p[i+1][2]-p[i][2])*f,1));
}
} // namespace Seis3D::Detail3D
