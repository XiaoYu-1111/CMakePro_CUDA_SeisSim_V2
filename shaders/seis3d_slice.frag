#version 430 core
in vec2 texCoord3D;
layout(location=0) out vec4 color3D;
uniform sampler2D sliceTexture3D;
uniform float scale3D;
uniform float gain3D;
uniform int component3D;
#include "seis3d_transfer.glsl"
// Composite a signed wavefield over independent, physically scaled geological material.
void main() {
    vec2 sample3D = texture(sliceTexture3D, texCoord3D).rg;
    float value3D = sample3D.r;
    float q3D = clamp(value3D * gain3D / max(scale3D, 1e-20), -1.0, 1.0);
    vec3 background3D = vec3(0.028, 0.045, 0.052);
    vec3 material3D=materialColor3D(sample3D.g);
    float alpha3D=smoothstep(waveThreshold3D,1.0,abs(q3D))*waveOpacity3D;
    vec3 base3D=displayMode3D==1?background3D:material3D;
    vec3 mapped3D=displayMode3D==2?material3D:mix(base3D,waveColor3D(q3D),alpha3D);
    color3D = vec4(mapped3D, 1.0);
}
