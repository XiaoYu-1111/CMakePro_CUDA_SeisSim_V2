#version 430 core
in vec2 texCoord3D;
layout(location=0) out vec4 color3D;
uniform sampler3D volumeTexture3D;
uniform vec3 extent3D;
uniform vec2 pan3D;
uniform float yaw3D, pitch3D, distance3D, scale3D, gain3D, opacity3D, threshold3D;
#include "seis3d_transfer.glsl"
// Raymarch a bounded auxiliary scalar volume with orbit, zoom and pan controlled by the workspace.
void main() {
    vec3 forward3D = normalize(vec3(cos(pitch3D)*sin(yaw3D),sin(pitch3D),cos(pitch3D)*cos(yaw3D)));
    vec3 right3D = normalize(cross(vec3(0,1,0),forward3D));
    vec3 up3D = cross(forward3D,right3D);
    vec3 target3D = pan3D.x*right3D + pan3D.y*up3D;
    vec3 origin3D = target3D + distance3D*forward3D;
    vec2 screen3D = (texCoord3D-0.5)*1.3;
    vec3 direction3D = normalize(-forward3D + screen3D.x*right3D + screen3D.y*up3D);
    vec3 safeDirection3D = mix(direction3D,vec3(1e-7),lessThan(abs(direction3D),vec3(1e-7)));
    vec3 inverse3D = 1.0/safeDirection3D;
    vec3 first3D = (-0.5*extent3D-origin3D)*inverse3D;
    vec3 second3D = (0.5*extent3D-origin3D)*inverse3D;
    vec3 near3D = min(first3D,second3D), far3D = max(first3D,second3D);
    float enter3D = max(max(near3D.x,near3D.y),near3D.z);
    float exit3D = min(min(far3D.x,far3D.y),far3D.z);
    vec3 background3D = vec3(0.025,0.041,0.05);
    if (exit3D <= max(enter3D,0.0)) { color3D=vec4(background3D,1); return; }
    enter3D = max(enter3D,0.0);
    vec4 accumulated3D = vec4(0);
    const int steps3D=192;
    float delta3D=(exit3D-enter3D)/float(steps3D);
    for (int i3D=0;i3D<steps3D;++i3D) {
        vec3 q3D=(origin3D+(enter3D+(float(i3D)+0.5)*delta3D)*direction3D)/extent3D+0.5;
        vec3 uvw3D=vec3(q3D.x,q3D.z,1.0-q3D.y); // Z depth points downward in the viewer.
        vec2 sample3D=texture(volumeTexture3D,uvw3D).rg;
        float scalar3D=sample3D.r*gain3D/max(scale3D,1e-20);
        float magnitude3D=abs(scalar3D);
        float waveDensity3D=displayMode3D==2?0.0:max(magnitude3D-max(threshold3D,waveThreshold3D),0.0)*opacity3D*waveOpacity3D*40.0;
        float materialDensity3D=displayMode3D==1?0.0:modelOpacity3D*3.0;
        float density3D=waveDensity3D+materialDensity3D;
        float alpha3D=1.0-exp(-density3D*delta3D);
        vec3 mapped3D=(waveColor3D(scalar3D)*waveDensity3D+materialColor3D(sample3D.g)*materialDensity3D)/max(density3D,1e-10);
        accumulated3D.rgb+=(1.0-accumulated3D.a)*alpha3D*mapped3D;
        accumulated3D.a+=(1.0-accumulated3D.a)*alpha3D;
        if (accumulated3D.a>0.98) break;
    }
    vec3 entry3D=(origin3D+enter3D*direction3D)/extent3D;
    vec3 edge3D=abs(abs(entry3D)-0.5);
    float secondEdge3D=max(min(edge3D.x,edge3D.y),min(max(edge3D.x,edge3D.y),edge3D.z));
    vec3 result3D=accumulated3D.rgb+(1.0-accumulated3D.a)*background3D;
    result3D=mix(result3D,vec3(0.35,0.76,0.38),1.0-smoothstep(0.002,0.006,secondEdge3D));
    color3D=vec4(result3D,1);
}
