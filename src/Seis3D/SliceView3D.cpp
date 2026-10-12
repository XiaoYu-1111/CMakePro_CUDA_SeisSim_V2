#include "Internal3D.h"
#include "AppTheme.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Seis3D::Detail3D {
namespace {
constexpr float maximumSliceZoom3D=32.0f;

struct SliceBounds3D {
    ImVec2 minimum3D,maximum3D;
};

// Constrain a normalized camera to the actual texture, including after zooming back out.
void constrainSliceCamera3D(SliceCamera3D& camera) {
    if(!std::isfinite(camera.zoom3D)) camera=SliceCamera3D{};
    camera.zoom3D=(std::clamp)(camera.zoom3D,1.0f,maximumSliceZoom3D);
    const float halfExtent=0.5f/camera.zoom3D;
    for(float& center:camera.center3D) {
        if(!std::isfinite(center)) center=0.5f;
        center=(std::clamp)(center,halfExtent,1.0f-halfExtent);
    }
}

// Use identical UV bounds for drawing, overlays and coordinate picking on all three planes.
SliceBounds3D sliceBounds3D(const SliceCamera3D& camera) {
    const float halfExtent=0.5f/camera.zoom3D;
    return {{camera.center3D[0]-halfExtent,camera.center3D[1]-halfExtent},
        {camera.center3D[0]+halfExtent,camera.center3D[1]+halfExtent}};
}

// Keep the sample beneath the pointer stationary while changing magnification, until an edge is met.
void zoomSliceAtPointer3D(SliceCamera3D& camera,ImVec2 pointer,float wheel) {
    const float oldExtent=1.0f/camera.zoom3D;
    const ImVec2 anchor(camera.center3D[0]+(pointer.x-0.5f)*oldExtent,
        camera.center3D[1]+(pointer.y-0.5f)*oldExtent);
    camera.zoom3D=(std::clamp)(camera.zoom3D*std::exp((std::clamp)(wheel,-20.0f,20.0f)*0.18f),1.0f,maximumSliceZoom3D);
    const float newExtent=1.0f/camera.zoom3D;
    camera.center3D[0]=anchor.x-(pointer.x-0.5f)*newExtent;
    camera.center3D[1]=anchor.y-(pointer.y-0.5f)*newExtent;
    constrainSliceCamera3D(camera);
}

// Transform physical cell-center positions through the same camera used to crop the slice texture.
ImVec2 projectSlicePoint3D(float x,float y,int width,int height,const SliceBounds3D& bounds,ImVec2 origin,ImVec2 size) {
    return {origin.x+((x+0.5f)/float(width)-bounds.minimum3D.x)*size.x/(bounds.maximum3D.x-bounds.minimum3D.x),
        origin.y+((y+0.5f)/float(height)-bounds.minimum3D.y)*size.y/(bounds.maximum3D.y-bounds.minimum3D.y)};
}

// Clip physical grids, linked crosshairs and source/receiver markers to the magnified view.
void drawSliceOverlay3D(const Seis3D_State& s,int plane,const SliceBounds3D& bounds,ImVec2 origin,ImVec2 size) {
    const auto& c=s.context3D;
    const int width=plane==1?c.ny3D:c.nx3D,height=plane==2?c.ny3D:c.nz3D;
    const int a=plane==1?s.slice3D[1]:s.slice3D[0],b=plane==2?s.slice3D[1]:s.slice3D[2];
    const bool lightCanvas=s.modelStyle3D==4;
    auto* draw=ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x+size.x,origin.y+size.y);
    draw->PushClipRect(origin,end,true);
    const ImVec2 crosshair=projectSlicePoint3D(float(a),float(b),width,height,bounds,origin,size);
    if(s.showCrosshair3D) {
        const auto crosshairColor=lightCanvas?IM_COL32(35,108,111,150):IM_COL32(149,221,146,150);
        draw->AddLine(ImVec2(crosshair.x,origin.y),ImVec2(crosshair.x,end.y),crosshairColor);
        draw->AddLine(ImVec2(origin.x,crosshair.y),ImVec2(end.x,crosshair.y),crosshairColor);
    }
    if(s.showGrid3D) {
        const ImU32 gridColor=ImGui::ColorConvertFloat4ToU32(lightCanvas?ImVec4(.25f,.36f,.38f,s.gridOpacity3D):ImVec4(.7f,.83f,.82f,s.gridOpacity3D));
        for(int i=1;i<5;++i) {
            const float fraction=float(i)/5;
            const float u=origin.x+(fraction-bounds.minimum3D.x)*size.x/(bounds.maximum3D.x-bounds.minimum3D.x);
            const float v=origin.y+(fraction-bounds.minimum3D.y)*size.y/(bounds.maximum3D.y-bounds.minimum3D.y);
            draw->AddLine(ImVec2(u,origin.y),ImVec2(u,end.y),gridColor);
            draw->AddLine(ImVec2(origin.x,v),ImVec2(end.x,v),gridColor);
        }
    }
    // Intersect the fixed plane before projecting, so unrelated 3-D points never appear on a slice.
    const auto project=[&](int px,int py,int pz,ImU32 color,float radius) {
        const bool intersects=plane==0?std::abs(py-s.slice3D[1])<=1:plane==1?std::abs(px-s.slice3D[0])<=1:std::abs(pz-s.slice3D[2])<=1;
        if(!intersects) return;
        const int u=plane==1?py:px,v=plane==2?py:pz;
        draw->AddCircleFilled(projectSlicePoint3D(float(u),float(v),width,height,bounds,origin,size),radius,color);
    };
    project(s.source3D.x3D,s.source3D.y3D,s.source3D.z3D,lightCanvas?IM_COL32(179,79,18,255):IM_COL32(255,188,69,255),4);
    for(const auto& receiver:s.receivers3D) project(receiver.x3D,receiver.y3D,receiver.z3D,lightCanvas?IM_COL32(20,111,143,230):IM_COL32(90,218,247,220),2);
    draw->PopClipRect();
    draw->AddRect(origin,end,AppUi::color(AppUi::palette().accent,.7f));
    // Compact physical rulers stay outside the image, keeping geological pixels unobscured.
    const float fontSize=(std::max)(10.0f,ImGui::GetFontSize()*.64f);
    const auto labelColor=AppUi::color(AppUi::palette().muted);
    for(int tick=0;tick<3;++tick) {
        const float fraction=float(tick)*.5f;
        const float u=bounds.minimum3D.x+fraction*(bounds.maximum3D.x-bounds.minimum3D.x);
        const float v=bounds.minimum3D.y+fraction*(bounds.maximum3D.y-bounds.minimum3D.y);
        const float x=(std::clamp)(u*float(width)-.5f,0.0f,float(width-1))*c.spacing3D;
        const float y=(std::clamp)(v*float(height)-.5f,0.0f,float(height-1))*c.spacing3D;
        char horizontal[32]{},vertical[32]{};
        std::snprintf(horizontal,sizeof(horizontal),"%.0f m",x);std::snprintf(vertical,sizeof(vertical),"%.0f",y);
        const auto horizontalSize=ImGui::GetFont()->CalcTextSizeA(fontSize,1e6f,0,horizontal);
        const auto verticalSize=ImGui::GetFont()->CalcTextSizeA(fontSize,1e6f,0,vertical);
        const float tx=origin.x+fraction*size.x,ty=origin.y+fraction*size.y;
        draw->AddLine(ImVec2(tx,end.y),ImVec2(tx,end.y+3),labelColor);
        draw->AddText(ImGui::GetFont(),fontSize,ImVec2((std::clamp)(tx-horizontalSize.x*.5f,origin.x,end.x-horizontalSize.x),end.y+4),labelColor,horizontal);
        draw->AddLine(ImVec2(origin.x-3,ty),ImVec2(origin.x,ty),labelColor);
        draw->AddText(ImGui::GetFont(),fontSize,ImVec2(origin.x-7-verticalSize.x,(std::clamp)(ty-fontSize*.5f,origin.y,end.y-fontSize)),labelColor,vertical);
    }
    const char* components[]={"Vz","Vx","Vy","|V|","Pressure"};
    char caption[64]{};std::snprintf(caption,sizeof(caption),"%s / %s",components[s.component3D],s.component3D==4?"Pa":"m/s");
    const auto captionSize=ImGui::GetFont()->CalcTextSizeA(fontSize,1e6f,0,caption);
    if(size.x>captionSize.x+16&&size.y>70) {
        draw->AddRectFilled(ImVec2(origin.x+5,origin.y+5),ImVec2(origin.x+captionSize.x+15,origin.y+fontSize+13),lightCanvas?IM_COL32(255,253,248,230):IM_COL32(7,13,20,205),3);
        draw->AddText(ImGui::GetFont(),fontSize,ImVec2(origin.x+10,origin.y+8),lightCanvas?IM_COL32(46,74,82,255):IM_COL32(201,219,230,255),caption);
    }
}
} // namespace

// Restore all orthogonal cameras without moving slice planes or modifying the simulated field.
void resetSliceViews3D(Seis3D_State& s) {
    for(auto& camera:s.sliceCameras3D) camera=SliceCamera3D{};
}

// Draw a full-resolution slice with pointer-anchored zoom, captured panning and physical picking.
void drawSlicePanel3D(Seis3D_State& s,int plane) {
    ImGui::PushID(plane);
    const bool visible=ImGui::BeginChild("slicePanel",ImVec2(0,0),ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    if(!visible) {ImGui::EndChild();ImGui::PopID();return;}
    if(!s.ready3D) {
        ImGui::TextWrapped("Initialize a 3D model to inspect this plane.");
        ImGui::EndChild();ImGui::PopID();return;
    }
    auto& camera=s.sliceCameras3D[plane];constrainSliceCamera3D(camera);
    const int axis=plane==0?1:plane==1?0:2;
    const int count=axis==0?s.context3D.nx3D:axis==1?s.context3D.ny3D:s.context3D.nz3D;
    ImGui::SetNextItemWidth(-1);
    if(ImGui::SliderInt("##position",s.slice3D+axis,0,count-1,"%d",ImGuiSliderFlags_AlwaysClamp)) s.dirty3D=true;
    if(ImGui::SmallButton("Reset view")) camera=SliceCamera3D{};
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Fit this slice to its panel. Keeps the current slice position and wavefield.");
    ImGui::SameLine();ImGui::TextDisabled("%.0f%% | %c = %.0f m",camera.zoom3D*100,'X'+axis,
        s.slice3D[axis]*s.context3D.spacing3D);
    const int width=s.gl3D.layout3D.sliceWidth3D[plane],height=s.gl3D.layout3D.sliceHeight3D[plane];
    const auto available=ImGui::GetContentRegionAvail();
    if(available.x<70||available.y<50) {ImGui::EndChild();ImGui::PopID();return;}
    const float ratio=float(width)/float(height);
    const ImVec2 imageArea(available.x-44,available.y-22);
    const ImVec2 size=imageArea.x/imageArea.y>ratio?ImVec2(imageArea.y*ratio,imageArea.y):ImVec2(imageArea.x,imageArea.x/ratio);
    const auto cursor=ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x+44+(imageArea.x-size.x)*.5f,cursor.y+(imageArea.y-size.y)*.5f));
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    // An ID-bearing canvas owns wheel input and continues a middle-button drag outside its bounds.
    ImGui::InvisibleButton("sliceCanvas3D",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const auto& io=ImGui::GetIO();
    const ImVec2 pointer((std::clamp)((io.MousePos.x-origin.x)/size.x,0.0f,1.0f),
        (std::clamp)((io.MousePos.y-origin.y)/size.y,0.0f,1.0f));
    if(hovered&&io.MouseWheel!=0) zoomSliceAtPointer3D(camera,pointer,io.MouseWheel);
    if(active&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        camera.center3D[0]-=io.MouseDelta.x/(size.x*camera.zoom3D);
        camera.center3D[1]-=io.MouseDelta.y/(size.y*camera.zoom3D);
        constrainSliceCamera3D(camera);
    }
    const auto bounds=sliceBounds3D(camera);
    ImGui::GetWindowDrawList()->AddImage(ImTextureID(s.gl3D.displayTextures3D[plane]),origin,
        ImVec2(origin.x+size.x,origin.y+size.y),bounds.minimum3D,bounds.maximum3D);
    drawSliceOverlay3D(s,plane,bounds,origin,size);
    if(hovered) {
        const int u=(std::clamp)(int((bounds.minimum3D.x+pointer.x*(bounds.maximum3D.x-bounds.minimum3D.x))*float(width)),0,width-1);
        const int v=(std::clamp)(int((bounds.minimum3D.y+pointer.y*(bounds.maximum3D.y-bounds.minimum3D.y))*float(height)),0,height-1);
        const Receiver3D point=plane==0?Receiver3D{u,s.slice3D[1],v}:plane==1?Receiver3D{s.slice3D[0],u,v}:Receiver3D{u,v,s.slice3D[2]};
        if(!s.hoverValid3D||point.x3D!=s.hoverPoint3D.x3D||point.y3D!=s.hoverPoint3D.y3D||point.z3D!=s.hoverPoint3D.z3D)
            performAction3D(s,[&]{
                probeMaterial3D(s.gpu3D,s.context3D,point,s.hoverMaterial3D);
                s.hoverPoint3D=point;s.hoverValid3D=true;
            });
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            s.slice3D[0]=point.x3D;s.slice3D[1]=point.y3D;s.slice3D[2]=point.z3D;s.dirty3D=true;
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)&&!ImGui::IsAnyMouseDown())
            ImGui::SetTooltip("Wheel: zoom at cursor (100-3200%%)\nMiddle drag: pan | Left click: link slices\nReset view: fit this slice | R: reset all views");
    }
    ImGui::EndChild();ImGui::PopID();
}
} // namespace Seis3D::Detail3D
