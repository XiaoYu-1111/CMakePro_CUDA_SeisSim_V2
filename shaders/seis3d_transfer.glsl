uniform vec3 palette3D[5];
uniform int displayMode3D, modelStyle3D, unsignedWave3D;
uniform vec2 materialRange3D;
uniform float waveOpacity3D, waveThreshold3D, modelOpacity3D;
uniform float waveGamma3D, materialBrightness3D;
// Adjust display opacity only; preserve the signed color mapping and physical sample amplitudes.
float waveVisibility3D(float magnitude3D) {
    float visible3D=clamp((magnitude3D-waveThreshold3D)/max(1.0-waveThreshold3D,1e-6),0.0,1.0);
    return pow(visible3D,max(waveGamma3D,0.1));
}
// Interpolate the same five stops used by the workspace legend.
vec3 waveColor3D(float value3D) {
    float q3D=clamp(unsignedWave3D!=0 ? abs(value3D) : value3D*0.5+0.5,0.0,1.0)*4.0;
    int index3D=min(int(q3D),3);
    return mix(palette3D[index3D],palette3D[index3D+1],q3D-float(index3D));
}
// Map true material values using a shared physical-unit range, including homogeneous models.
vec3 materialColor3D(float value3D) {
    float span3D=materialRange3D.y-materialRange3D.x;
    float q3D=span3D>max(1e-6,abs(materialRange3D.y)*1e-6)
        ? clamp((value3D-materialRange3D.x)/span3D,0.0,1.0) : 0.5;
    if(modelStyle3D==0) return mix(vec3(.09,.13,.17),vec3(.43,.53,.6),q3D);
    if(modelStyle3D==1) return q3D<.5 ? mix(vec3(.13,.28,.33),vec3(.48,.65,.46),q3D*2.0)
        : mix(vec3(.48,.65,.46),vec3(.78,.52,.31),(q3D-.5)*2.0);
    if(modelStyle3D==2) return vec3(mix(.12,.85,q3D));
    if(modelStyle3D==3) return waveColor3D(unsignedWave3D!=0?q3D:q3D*2.0-1.0);
    return mix(vec3(.97,.96,.93),vec3(.79,.84,.85),q3D);
}
