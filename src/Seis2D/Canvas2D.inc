// Private 2D implementation, compiled only through Simulation2D.cpp.
// Draw physical ruler labels behind tool windows, preserving the model-to-screen mapping.
inline void renderGridRulerOnBackbuffer(
    const SimulationContext& simulationContext2D,
    const SimState& state,
    int                      winW,
    int                      winH,
    float                    barHeight,
    const ImVec2& viewOffset2D,
    float                    viewZoom2D
) {
    ImDrawList* draw_list = canvasDrawList2D;
    ImU32       textCol = AppUi::color(AppUi::palette().muted); 
    ImU32       tickCol = AppUi::color(AppUi::palette().muted,.5f);

    float  winAspect = (float)winW / (float)winH;
    float  simAspect = (float)simulationContext2D.NX / (float)simulationContext2D.NZ;
    ImVec2 aspectCorr = { 1.0f, 1.0f };

    if (winAspect > simAspect) {
        aspectCorr.x = winAspect / simAspect;
    }
    else {
        aspectCorr.y = simAspect / winAspect;
    }

    auto calcNiceInterval = [&](float target_pixel_dist, float total_cells, float aspect_corr, float win_dim) -> float {
        
        float target_phys = (target_pixel_dist * total_cells * aspect_corr * simulationContext2D.h) / (win_dim * viewZoom2D);
        if (target_phys <= 0.0f) return 10.0f;

        float log_val = log10f(target_phys);
        float base_power = powf(10.0f, floorf(log_val));
        float ratio = target_phys / base_power;

        float nice_interval = base_power;
        if (ratio > 5.0f)      nice_interval = base_power * 5.0f;
        else if (ratio > 2.0f) nice_interval = base_power * 2.0f;

        return (nice_interval < 1.0f) ? 1.0f : nice_interval;
        };

    float nice_interval_x = calcNiceInterval(100.0f, simulationContext2D.NX, aspectCorr.x, (float)winW);
    float nice_interval_y = calcNiceInterval(100.0f, simulationContext2D.NZ, aspectCorr.y, (float)winH);

    float total_width_m = simulationContext2D.NX * simulationContext2D.h;
    for (float physX = 0.0f; physX <= total_width_m; physX += nice_interval_x) {
        
        float simX = physX / simulationContext2D.h;
        float aPos_x = (simX / (float)simulationContext2D.NX) * 2.0f - 1.0f;
        float Pndc_x = (aPos_x - viewOffset2D.x) * viewZoom2D / aspectCorr.x;
        float screenX = (Pndc_x * 0.5f + 0.5f) * winW;

        if (screenX >= 0.0f && screenX <= (float)winW) {
            float screenY = barHeight + 2.0f;
            draw_list->AddLine(ImVec2(canvasOrigin2D.x+screenX,canvasOrigin2D.y+screenY), ImVec2(canvasOrigin2D.x+screenX,canvasOrigin2D.y+screenY+6.0f), tickCol, 1.5f);

            char buf[32];
            snprintf(buf, 32, "%.0fm", physX);

            ImVec2 txtSize = AppUi::Widgets::CalcTextSize(buf);
            float  offsetX = -txtSize.x * 0.5f;

            ImVec2 pos(canvasOrigin2D.x+std::clamp(screenX+offsetX,0.0f,std::max(0.0f,winW-txtSize.x)),canvasOrigin2D.y+screenY+8.0f);
            if (!AppUi::palette().light) AppUi::Widgets::drawText(draw_list,ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), buf);
            AppUi::Widgets::drawText(draw_list,pos, textCol, buf);
        }
    }

    float total_depth_m = simulationContext2D.NZ * simulationContext2D.h;
    for (float physY = 0.0f; physY <= total_depth_m; physY += nice_interval_y) {
        float simY = physY / simulationContext2D.h;
        float aPos_y = 1.0f - (simY / (float)simulationContext2D.NZ) * 2.0f;
        float Pndc_y = (aPos_y - viewOffset2D.y) * viewZoom2D / aspectCorr.y;
        float screenY = (0.5f - Pndc_y * 0.5f) * winH;

        if (screenY >= barHeight && screenY <= (float)winH) {
            float screenX = 4.0f;
            draw_list->AddLine(ImVec2(canvasOrigin2D.x+screenX,canvasOrigin2D.y+screenY), ImVec2(canvasOrigin2D.x+screenX+6.0f,canvasOrigin2D.y+screenY), tickCol, 1.5f);

            char buf[32];
            snprintf(buf, 32, "%.0fm", physY);

            ImVec2 txtSize = AppUi::Widgets::CalcTextSize(buf);
            ImVec2 pos(canvasOrigin2D.x+screenX+8.0f,canvasOrigin2D.y+std::clamp(screenY-txtSize.y*.5f,0.0f,std::max(0.0f,winH-txtSize.y)));

            if (!AppUi::palette().light) AppUi::Widgets::drawText(draw_list,ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), buf);
            AppUi::Widgets::drawText(draw_list,pos, textCol, buf);
        }
    }
}

inline void renderBrushCursor(const SimState& state, const ViewportInfo& vp) {
    ImVec2 mousePos = ImGui::GetMousePos();
    bool   isInside = (mousePos.x >= vp.x && mousePos.x < (vp.x + vp.w) &&
        mousePos.y >= vp.y && mousePos.y < (vp.y + vp.h));

    if (!canvasHovered2D || acquisition2D.isRecording) return;

    if (isInside) {
        ImDrawList* fg = canvasDrawList2D;

        if (state.brushType == TOOL_NONE) {
            float t = (float)ImGui::GetTime();
            const float crosshairSize = 18.0f;
            const float outerRingRadius = 35.0f;

            float r = uiAccent2D.x;
            float g = uiAccent2D.y;
            float b = uiAccent2D.z;

            ImU32 col_main = IM_COL32(r * 255, g * 255, b * 255, 255);

            ImU32 col_dynamic = IM_COL32(
                (int)std::min(255.0f, r * 1.15f * 255.0f),
                (int)std::min(255.0f, g * 1.15f * 255.0f),
                (int)std::min(255.0f, b * 1.15f * 255.0f),
                255
            );

            ImU32 col_shadow = IM_COL32(10, 18, 12, 160);

            fg->AddLine(ImVec2(mousePos.x - crosshairSize, mousePos.y), ImVec2(mousePos.x + crosshairSize, mousePos.y), col_shadow, 4.0f);
            fg->AddLine(ImVec2(mousePos.x, mousePos.y - crosshairSize), ImVec2(mousePos.x, mousePos.y + crosshairSize), col_shadow, 4.0f);

            fg->AddLine(ImVec2(mousePos.x - crosshairSize, mousePos.y), ImVec2(mousePos.x + crosshairSize, mousePos.y), col_main, 2.0f);
            fg->AddLine(ImVec2(mousePos.x, mousePos.y - crosshairSize), ImVec2(mousePos.x, mousePos.y + crosshairSize), col_main, 2.0f);

            fg->AddCircle(mousePos, outerRingRadius, col_main, 32, 1.5f);

            float  angle = t * 4.0f;
            ImVec2 scanEnd = ImVec2(mousePos.x + cos(angle) * outerRingRadius,
                mousePos.y + sin(angle) * outerRingRadius);
            fg->AddLine(mousePos, scanEnd, col_dynamic, 2.0f);
            fg->AddCircleFilled(scanEnd, 3.5f, col_dynamic);

            float pulse_t = fmodf(t, 1.5f) / 1.5f;
            float pulse_radius = pulse_t * outerRingRadius;
            int   pulse_alpha = (int)(sin(pulse_t * 3.14159f) * 120);

            ImU32 col_pulse = IM_COL32(r * 255, g * 255, b * 255, pulse_alpha);
            fg->AddCircle(mousePos, pulse_radius, col_pulse, 32, 3.0f);
        }
        else {
            ImU32 brushColor;
            switch (state.brushType) {
            case TOOL_HIGH:   brushColor = IM_COL32(255, 100, 100, 200); break;
            case TOOL_LOW:    brushColor = IM_COL32(100, 150, 255, 200); break;
            case TOOL_WALL:   brushColor = IM_COL32(255, 255, 0, 200);   break;
            case TOOL_ERASER: brushColor = IM_COL32(200, 200, 200, 200); break;
            default:          brushColor = IM_COL32(255, 255, 255, 200); break;
            }

            float screenRadius = state.brushRadius * vp.scaleX;
            fg->AddCircle(mousePos, screenRadius, brushColor, 0, 2.0f);
            fg->AddCircle(mousePos, screenRadius - 1.0f, IM_COL32(0, 0, 0, 150), 0, 1.0f);
            fg->AddCircleFilled(mousePos, 3.0f, IM_COL32(255, 255, 255, 255));
        }
    }
}

inline void renderSourceMarker(
    const SimulationContext& simulationContext2D,
    int                      src_x,
    int                      src_z,
    int                      winW,
    int                      winH,
    float                    barHeight,
    const ImVec2& viewOffset2D,
    float                    viewZoom2D
) {
    float  winAspect = (float)winW / (float)winH;
    float  simAspect = (float)simulationContext2D.NX / (float)simulationContext2D.NZ;
    ImVec2 aspectCorr = { 1.0f, 1.0f };

    if (winAspect > simAspect) {
        aspectCorr.x = winAspect / simAspect;
    }
    else {
        aspectCorr.y = simAspect / winAspect;
    }

    float aPos_x = ((float)src_x / (float)simulationContext2D.NX) * 2.0f - 1.0f;
    float aPos_y = 1.0f - ((float)src_z / (float)simulationContext2D.NZ) * 2.0f;

    float Pndc_x = (aPos_x - viewOffset2D.x) * viewZoom2D / aspectCorr.x;
    float screenX = (Pndc_x * 0.5f + 0.5f) * winW;

    float Pndc_y = (aPos_y - viewOffset2D.y) * viewZoom2D / aspectCorr.y;
    float screenY = (0.5f - Pndc_y * 0.5f) * winH;

    if (screenX >= 0.0f && screenX <= winW && screenY >= barHeight && screenY <= winH) {
        ImDrawList* fg = canvasDrawList2D;
        ImVec2      pos(canvasOrigin2D.x+screenX,canvasOrigin2D.y+screenY);

        ImU32 colMain = AppUi::color(AppUi::functionColor(AppUi::FunctionRole::Source));
        ImU32 colBlack = IM_COL32(0, 0, 0, 180);
        float lineLen = 20.0f;
        float gap = 3.0f;

        fg->AddLine(ImVec2(pos.x - lineLen, pos.y), ImVec2(pos.x + lineLen, pos.y), colBlack, 3.5f);
        fg->AddLine(ImVec2(pos.x, pos.y - lineLen), ImVec2(pos.x, pos.y + lineLen), colBlack, 3.5f);

        fg->AddLine(ImVec2(pos.x - lineLen, pos.y), ImVec2(pos.x - gap, pos.y), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x + gap, pos.y), ImVec2(pos.x + lineLen, pos.y), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x, pos.y - lineLen), ImVec2(pos.x, pos.y - gap), colMain, 1.5f);
        fg->AddLine(ImVec2(pos.x, pos.y + gap), ImVec2(pos.x, pos.y + lineLen), colMain, 1.5f);

        char buf[64];
        snprintf(buf, 64, "%d, %d (%.0fm, %.0fm)", src_x, src_z, src_x * simulationContext2D.h, src_z * simulationContext2D.h);

        ImVec2 textPos = ImVec2(pos.x + 8, pos.y + 8);
        if(!AppUi::palette().light) AppUi::Widgets::drawText(fg,ImVec2(textPos.x + 1, textPos.y + 1), colBlack, buf);
        AppUi::Widgets::drawText(fg,textPos, colMain, buf);

        float t = (float)ImGui::GetTime();
        float pulse = (sin(t * 3.0f) * 0.5f + 0.5f) * 5.0f;
        fg->AddCircle(pos, 5.0f + pulse, AppUi::color(AppUi::functionColor(AppUi::FunctionRole::Source),.6f), 16, 1.0f);
    }
}

inline void renderSeisPopups() {

    if (showSuccessPopup2D) {
        ImGui::OpenPopup("Simulation Success");
        showSuccessPopup2D = false; 
    }

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::BeginPopupModal("Simulation Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        AppUi::Widgets::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[ SUCCESS ]");
        ImGui::Separator();
        ImGui::Spacing();

        AppUi::Widgets::Text("%s", popupMessage2D.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (AppUi::Widgets::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (showErrorPopup2D) {
        ImGui::OpenPopup("Simulation Error");
        showErrorPopup2D = false; 
    }

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::BeginPopupModal("Simulation Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        AppUi::Widgets::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ ERROR ]");
        ImGui::Separator();
        ImGui::Spacing();

        AppUi::Widgets::Text("%s", popupMessage2D.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (AppUi::Widgets::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

