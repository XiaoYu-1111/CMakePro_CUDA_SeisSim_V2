#include "Internal3D.h"

namespace Seis3D::Detail3D {

// Replace only procedural material/geometry settings; keep the chosen grid, boundary and acquisition.
void applyModelPreset3D(SimulationContext3D& c,ModelKind3D kind) {
    c.modelKind3D=kind;
    c.vp3D=3000;c.vs3D=1700;c.density3D=2200;
    c.layerVp3D=4200;c.layerVs3D=2400;c.layerDensity3D=2500;
    c.interfaceDepth3D=.5f;c.layerDip3D=.25f;c.layerThickness3D=.15f;
    c.faultThrow3D=.18f;c.anomalyRadius3D=.187f;
    if(kind==ModelKind3D::LowVelocityLayer) {
        c.vp3D=4200;c.vs3D=2400;c.density3D=2500;
        c.layerVp3D=2400;c.layerVs3D=1200;c.layerDensity3D=2100;
    }
    if(kind==ModelKind3D::SaltDome) {
        c.layerVp3D=4500;c.layerVs3D=2600;c.layerDensity3D=2200;c.anomalyRadius3D=.22f;
    }
}

// Explain the selected synthetic geometry without implying a measured field model or benchmark.
const char* modelDescription3D(ModelKind3D kind) {
    switch(kind) {
    case ModelKind3D::Uniform:return "Homogeneous reference medium for wavefront and boundary comparisons.";
    case ModelKind3D::TwoLayer:return "Horizontal interface between background and secondary material.";
    case ModelKind3D::Sphere:return "Centered ellipsoidal inclusion; radius is a fraction of each axis.";
    case ModelKind3D::ThreeLayer:return "Three horizontal layers: background, midpoint properties, secondary material.";
    case ModelKind3D::DippingLayer:return "Planar interface dipping along X; depth varies across the model.";
    case ModelKind3D::LowVelocityLayer:return "Slow horizontal interbed in a faster host; thickness is adjustable.";
    case ModelKind3D::Fault:return "Dipping interface offset across X=midpoint; adjustable vertical throw.";
    case ModelKind3D::Gradient:return "Vp, Vs and density increase linearly from top to bottom endpoints.";
    case ModelKind3D::SaltDome:return "Synthetic high-velocity dome extending upward from the model bottom.";
    default:return "Imported sampled properties. Choose a preset to create a new procedural model.";
    }
}

} // namespace Seis3D::Detail3D
