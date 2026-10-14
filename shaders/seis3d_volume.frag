#version 430 core
in vec2 texCoord3D;
layout(location=0) out vec4 color3D;
uniform sampler3D volumeTexture3D;
uniform vec3 extent3D;
uniform vec2 pan3D;
uniform vec3 domainLow3D, domainHigh3D, physicalLow3D, physicalHigh3D;
uniform bool showBoundary3D;
uniform float boundaryOpacity3D;
uniform float yaw3D, pitch3D, distance3D, scale3D, gain3D, opacity3D, threshold3D;
uniform float aspect3D, sampling3D, sharpness3D;
uniform float volumeGain3D, opticalScale3D;
#include "seis3d_transfer.glsl"
// Raymarch a bounded auxiliary scalar volume with orbit, zoom and pan controlled by the workspace.
void main() {
    vec3 forward3D = normalize(vec3(cos(pitch3D)*sin(yaw3D),sin(pitch3D),cos(pitch3D)*cos(yaw3D)));
    vec3 right3D = normalize(cross(vec3(0,1,0),forward3D));
    vec3 up3D = cross(forward3D,right3D);
    vec3 target3D = pan3D.x*right3D + pan3D.y*up3D;
    vec3 origin3D = target3D + distance3D*forward3D;
    vec2 screen3D = (texCoord3D-0.5)*1.3;
    screen3D.x*=aspect3D;
    vec3 direction3D = normalize(-forward3D + screen3D.x*right3D + screen3D.y*up3D);
    vec3 safeDirection3D = mix(direction3D,vec3(1e-7),lessThan(abs(direction3D),vec3(1e-7)));
    vec3 inverse3D = 1.0/safeDirection3D;
    vec3 first3D = (-0.5*extent3D-origin3D)*inverse3D;
    vec3 second3D = (0.5*extent3D-origin3D)*inverse3D;
    vec3 near3D = min(first3D,second3D), far3D = max(first3D,second3D);
    float enter3D = max(max(near3D.x,near3D.y),near3D.z);
    float exit3D = min(min(far3D.x,far3D.y),far3D.z);
    vec3 background3D = modelStyle3D==4?vec3(.97,.96,.93):vec3(0.025,0.041,0.05);
    if (exit3D <= max(enter3D,0.0)) { color3D=vec4(background3D,1); return; }
    enter3D = max(enter3D,0.0);
    vec4 accumulated3D = vec4(0);
    // Follow the traversed voxel distance rather than stretching 192 samples over every grid.
    vec3 path3D=abs(direction3D*(exit3D-enter3D)/extent3D);
    vec3 texturePath3D=vec3(path3D.x,path3D.z,path3D.y)*(domainHigh3D-domainLow3D);
    int steps3D=int(clamp(ceil(length(texturePath3D*vec3(textureSize(volumeTexture3D,0)))*sampling3D),192.0,1024.0));
    float delta3D=(exit3D-enter3D)/float(steps3D);
    for (int i3D=0;i3D<steps3D;++i3D) {
        vec3 q3D=(origin3D+(enter3D+(float(i3D)+0.5)*delta3D)*direction3D)/extent3D+0.5;
        vec3 uvw3D=vec3(q3D.x,q3D.z,1.0-q3D.y); // Z depth points downward in the viewer.
        uvw3D=mix(domainLow3D,domainHigh3D,uvw3D);
        bool boundary3D=showBoundary3D&&(any(lessThan(uvw3D,physicalLow3D))||any(greaterThan(uvw3D,physicalHigh3D)));
        vec2 sample3D=texture(volumeTexture3D,uvw3D).rg;
        float scalar3D=sample3D.r*gain3D*volumeGain3D/max(scale3D,1e-20);
        float magnitude3D=abs(scalar3D);
        // Opacity is specified per reference grid distance, rather than per normalized display box.
        // Material opacity remains relative to the box so large models do not turn into opaque blocks.
        float waveDensity3D=displayMode3D==2||magnitude3D<=threshold3D?0.0:pow(waveVisibility3D(magnitude3D),sharpness3D)*opacity3D*waveOpacity3D*28.0*opticalScale3D;
        float materialDensity3D=boundary3D?boundaryOpacity3D*1.8:displayMode3D==1?0.0:modelOpacity3D*3.0;
        float density3D=waveDensity3D+materialDensity3D;
        float alpha3D=1.0-exp(-density3D*delta3D);
        vec3 materialTint3D=boundary3D?vec3(.78,.53,.20):materialColor3D(sample3D.g);
        vec3 mapped3D=(waveColor3D(scalar3D)*waveDensity3D+materialTint3D*materialDensity3D)/max(density3D,1e-10);
        accumulated3D.rgb+=(1.0-accumulated3D.a)*alpha3D*mapped3D;
        accumulated3D.a+=(1.0-accumulated3D.a)*alpha3D;
        if (accumulated3D.a>0.98) break;
    }
    vec3 entry3D=(origin3D+enter3D*direction3D)/extent3D;
    vec3 edge3D=abs(abs(entry3D)-0.5);
    float secondEdge3D=max(min(edge3D.x,edge3D.y),min(max(edge3D.x,edge3D.y),edge3D.z));
    vec3 result3D=accumulated3D.rgb+(1.0-accumulated3D.a)*background3D;
    vec3 outline3D=modelStyle3D==4?vec3(.22,.43,.46):vec3(0.35,0.76,0.38);
    bool hasLayer3D=showBoundary3D&&any(lessThan(physicalHigh3D-physicalLow3D,vec3(.999)));
    result3D=mix(result3D,hasLayer3D?vec3(.78,.53,.20):outline3D,1.0-smoothstep(0.002,0.006,secondEdge3D));
    if(hasLayer3D) {
        // Draw the actual physical box inside the CPML shell, using the same depth-axis convention.
        vec3 physicalMin3D=(vec3(physicalLow3D.x,1.0-physicalHigh3D.z,physicalLow3D.y)-.5)*extent3D;
        vec3 physicalMax3D=(vec3(physicalHigh3D.x,1.0-physicalLow3D.z,physicalHigh3D.y)-.5)*extent3D;
        vec3 a3D=(physicalMin3D-origin3D)*inverse3D,b3D=(physicalMax3D-origin3D)*inverse3D;
        vec3 nearBox3D=min(a3D,b3D),farBox3D=max(a3D,b3D);
        float boxEnter3D=max(max(nearBox3D.x,nearBox3D.y),nearBox3D.z);
        float boxExit3D=min(min(farBox3D.x,farBox3D.y),farBox3D.z);
        if(boxExit3D>max(boxEnter3D,0.0)) {
            vec3 p3D=(origin3D+boxEnter3D*direction3D-(physicalMin3D+physicalMax3D)*.5)/(physicalMax3D-physicalMin3D);
            vec3 e3D=abs(abs(p3D)-.5);
            float edgeDistance3D=max(min(e3D.x,e3D.y),min(max(e3D.x,e3D.y),e3D.z));
            result3D=mix(result3D,outline3D,1.0-smoothstep(.002,.006,edgeDistance3D));
        }
    }
    color3D=vec4(result3D,1);
}
