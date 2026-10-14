#include "AppWidgets.h"
#include "AppTheme.h"
#include "ResourcePaths.h"
#include "implot.h"
#include <fstream>

namespace AppUi {
namespace {
ThemeId selectedTheme=ThemeId::CudaGreen;
bool savedPreference=true;
constexpr const char* themeNames[]={"CUDA Green","Ocean Blue","Graphite","Scientific Light",
    "Midnight Violet","Teal Studio","Amber Lab","Paper Sand","Arctic Lab","Warm Ivory"};
const ThemePalette palettes[]={
    {{.035f,.055f,.052f,1},{.065f,.095f,.09f,1},{.09f,.14f,.12f,1},{.10f,.17f,.14f,1},{.91f,.96f,.93f,1},{.55f,.68f,.62f,1},{.46f,.86f,.27f,1},{.22f,.4f,.29f,1},false},
    {{.035f,.052f,.08f,1},{.06f,.09f,.135f,1},{.085f,.14f,.20f,1},{.09f,.16f,.23f,1},{.91f,.95f,.99f,1},{.54f,.66f,.79f,1},{.22f,.68f,.96f,1},{.2f,.36f,.52f,1},false},
    {{.065f,.07f,.08f,1},{.105f,.115f,.13f,1},{.15f,.16f,.18f,1},{.17f,.18f,.20f,1},{.93f,.94f,.96f,1},{.6f,.63f,.68f,1},{.72f,.77f,.84f,1},{.31f,.34f,.39f,1},false},
    {{.93f,.95f,.97f,1},{.99f,.995f,1,1},{.88f,.92f,.95f,1},{.88f,.92f,.96f,1},{.12f,.18f,.24f,1},{.35f,.43f,.51f,1},{.035f,.36f,.64f,1},{.65f,.74f,.82f,1},true},
    {{.045f,.04f,.08f,1},{.085f,.075f,.135f,1},{.13f,.115f,.20f,1},{.16f,.14f,.245f,1},{.95f,.93f,.99f,1},{.65f,.61f,.77f,1},{.69f,.54f,.97f,1},{.32f,.27f,.45f,1},false},
    {{.025f,.065f,.075f,1},{.045f,.105f,.12f,1},{.065f,.16f,.175f,1},{.08f,.19f,.205f,1},{.91f,.97f,.97f,1},{.54f,.71f,.73f,1},{.16f,.80f,.76f,1},{.18f,.40f,.42f,1},false},
    {{.07f,.055f,.035f,1},{.12f,.095f,.065f,1},{.18f,.145f,.10f,1},{.21f,.17f,.12f,1},{.98f,.95f,.89f,1},{.74f,.67f,.54f,1},{.96f,.68f,.24f,1},{.43f,.34f,.21f,1},false},
    {{.96f,.945f,.91f,1},{.995f,.985f,.96f,1},{.92f,.895f,.84f,1},{.92f,.905f,.865f,1},{.16f,.20f,.19f,1},{.36f,.41f,.38f,1},{.10f,.40f,.32f,1},{.68f,.70f,.63f,1},true},
    {{.935f,.955f,.975f,1},{.995f,.998f,1,1},{.88f,.925f,.965f,1},{.91f,.945f,.975f,1},{.10f,.17f,.26f,1},{.34f,.43f,.53f,1},{.13f,.34f,.66f,1},{.65f,.74f,.83f,1},true},
    {{.97f,.95f,.92f,1},{1,.99f,.97f,1},{.94f,.90f,.84f,1},{.96f,.935f,.89f,1},{.22f,.18f,.15f,1},{.44f,.38f,.32f,1},{.64f,.31f,.12f,1},{.76f,.69f,.59f,1},true}
};
static_assert(sizeof(themeNames)/sizeof(*themeNames)==int(ThemeId::Count));
static_assert(sizeof(palettes)/sizeof(*palettes)==int(ThemeId::Count));

// Blend opaque semantic colors for consistent control states across light and dark palettes.
ImVec4 blend(ImVec4 a,ImVec4 b,float weight) {
    return {a.x+(b.x-a.x)*weight,a.y+(b.y-a.y)*weight,a.z+(b.z-a.z)*weight,1};
}

// Apply semantic colors while retaining the application's font sizes and spacing configuration.
void configureThemeStyle(ImGuiStyle& style) {
    const auto& p=palette();
    if(p.light) ImGui::StyleColorsLight(&style);else ImGui::StyleColorsDark(&style);
    style.FrameRounding=4;style.GrabRounding=3;style.TabRounding=4;
    style.ChildRounding=5;style.PopupRounding=6;style.ScrollbarRounding=6;
    style.FrameBorderSize=1;style.ChildBorderSize=1;style.DisabledAlpha=.68f;
    auto* c=style.Colors;
    c[ImGuiCol_Text]=p.text;c[ImGuiCol_TextDisabled]=p.muted;
    c[ImGuiCol_WindowBg]=p.background;c[ImGuiCol_ChildBg]=p.panel;c[ImGuiCol_PopupBg]=p.panel;
    c[ImGuiCol_Border]=p.border;c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
    c[ImGuiCol_TitleBg]=p.panel;c[ImGuiCol_TitleBgActive]=p.raised;c[ImGuiCol_TitleBgCollapsed]=p.panel;
    c[ImGuiCol_FrameBg]=p.input;c[ImGuiCol_FrameBgHovered]=blend(p.input,p.accent,.2f);
    c[ImGuiCol_FrameBgActive]=blend(p.input,p.accent,.35f);
    c[ImGuiCol_Button]=blend(p.raised,p.accent,p.light?.12f:.15f);
    c[ImGuiCol_ButtonHovered]=blend(p.raised,p.accent,.35f);c[ImGuiCol_ButtonActive]=blend(p.raised,p.accent,.5f);
    c[ImGuiCol_CheckMark]=p.accent;c[ImGuiCol_SliderGrab]=p.accent;c[ImGuiCol_SliderGrabActive]=blend(p.accent,p.text,.2f);
    c[ImGuiCol_Header]=blend(p.panel,p.accent,.18f);c[ImGuiCol_HeaderHovered]=blend(p.panel,p.accent,.3f);
    c[ImGuiCol_HeaderActive]=blend(p.panel,p.accent,.42f);
    c[ImGuiCol_Tab]=p.raised;c[ImGuiCol_TabHovered]=blend(p.panel,p.accent,.4f);
    c[ImGuiCol_TabSelected]=blend(p.panel,p.accent,.24f);c[ImGuiCol_TabDimmed]=p.panel;
    c[ImGuiCol_TabDimmedSelected]=p.raised;c[ImGuiCol_TabSelectedOverline]=p.accent;
    c[ImGuiCol_TabDimmedSelectedOverline]=p.border;
    c[ImGuiCol_Separator]=p.border;c[ImGuiCol_SeparatorHovered]=p.accent;c[ImGuiCol_SeparatorActive]=p.accent;
    c[ImGuiCol_ResizeGrip]=ImVec4(p.accent.x,p.accent.y,p.accent.z,.2f);
    c[ImGuiCol_ResizeGripHovered]=p.accent;c[ImGuiCol_ResizeGripActive]=p.accent;
    c[ImGuiCol_ScrollbarBg]=p.background;c[ImGuiCol_ScrollbarGrab]=p.border;
    c[ImGuiCol_ScrollbarGrabHovered]=p.muted;c[ImGuiCol_ScrollbarGrabActive]=p.accent;
    c[ImGuiCol_DockingPreview]=ImVec4(p.accent.x,p.accent.y,p.accent.z,.5f);
    c[ImGuiCol_DockingEmptyBg]=p.background;c[ImGuiCol_NavCursor]=p.accent;
    c[ImGuiCol_TextSelectedBg]=ImVec4(p.accent.x,p.accent.y,p.accent.z,.3f);
}
// Apply the selected presentation style to the application and refresh ImPlot's automatic colors.
void applyTheme() {configureThemeStyle(ImGui::GetStyle());if(ImPlot::GetCurrentContext()) ImPlot::StyleColorsAuto();}
} // namespace

// Expose only presentation state; no simulation module owns this preference.
ThemeId currentTheme() {return selectedTheme;}
// Reject invalid persisted values before indexing the palette array.
const ThemePalette& themePalette(ThemeId id) {
    const int index=int(id);return palettes[index>=0&&index<int(ThemeId::Count)?index:0];
}
// Read the palette selected by the application toolbar.
const ThemePalette& palette() {return themePalette(selectedTheme);}
// Apply opacity consistently for both light and dark draw-list surfaces.
ImU32 color(ImVec4 value,float opacity) {value.w*=opacity;return ImGui::ColorConvertFloat4ToU32(value);}

// Keep functional colors consistent across themes while retaining readable luminance on each surface.
ImVec4 functionColor(FunctionRole role) {
    static const ImVec4 dark[]={{.36f,.83f,.51f,1},{1,.66f,.32f,1},{.37f,.69f,1,1},{.76f,.60f,1,1},
        {.22f,.80f,.80f,1},{.66f,.73f,.82f,1},{1,.78f,.29f,1},{1,.43f,.43f,1}};
    static const ImVec4 light[]={{.09f,.43f,.25f,1},{.70f,.32f,.07f,1},{.12f,.36f,.67f,1},{.49f,.29f,.68f,1},
        {.04f,.43f,.47f,1},{.34f,.40f,.49f,1},{.60f,.40f,.05f,1},{.72f,.20f,.21f,1}};
    const int i=static_cast<int>(role);
    return (palette().light?light:dark)[i>=0&&i<8?i:5];
}

// Scope all purpose-dependent widget colors; labels retain the theme's high-contrast foreground.
FunctionStyleScope::FunctionStyleScope(FunctionRole role,bool prominent) {
    themeOnEntry=currentTheme();
    const auto& p=palette();const auto accent=functionColor(role);
    const auto push=[&](ImGuiCol slot,ImVec4 value){ImGui::PushStyleColor(slot,value);++colorCount;};
    push(ImGuiCol_Button,blend(p.raised,accent,prominent?.28f:.14f));
    push(ImGuiCol_ButtonHovered,blend(p.raised,accent,.32f));
    push(ImGuiCol_ButtonActive,blend(p.raised,accent,.43f));
    push(ImGuiCol_Border,blend(p.border,accent,.38f));
    push(ImGuiCol_Header,blend(p.panel,accent,.14f));
    push(ImGuiCol_HeaderHovered,blend(p.panel,accent,.24f));
    push(ImGuiCol_HeaderActive,blend(p.panel,accent,.33f));
    push(ImGuiCol_Tab,blend(p.raised,accent,.10f));
    push(ImGuiCol_TabHovered,blend(p.panel,accent,.25f));
    push(ImGuiCol_TabSelected,blend(p.panel,accent,.19f));
    push(ImGuiCol_TabSelectedOverline,accent);
    push(ImGuiCol_TabDimmedSelectedOverline,blend(p.border,accent,.6f));
    push(ImGuiCol_CheckMark,accent);push(ImGuiCol_SliderGrab,accent);
    push(ImGuiCol_SliderGrabActive,blend(accent,p.text,.15f));
    push(ImGuiCol_FrameBgHovered,blend(p.input,accent,.12f));
    push(ImGuiCol_FrameBgActive,blend(p.input,accent,.20f));
}

// Return every pushed color even when a panel exits early or an operation throws.
FunctionStyleScope::~FunctionStyleScope() {
    ImGui::PopStyleColor(colorCount);
    if(currentTheme()!=themeOnEntry) applyTheme();
}

// Load only a known version and bounded theme index; missing or malformed files retain CUDA Green.
void initializeTheme() {
    std::ifstream input(getExecutableDirectory()/"ui_theme.ini");
    std::string signature;int index=-1;input>>signature>>index;
    selectedTheme=signature=="SeisUiTheme1"&&index>=0&&index<int(ThemeId::Count)?ThemeId(index):ThemeId::CudaGreen;
    applyTheme();
}
// Persist a small, versioned preference beside the executable, keeping generated files under outer out.
void setTheme(ThemeId id,bool persist) {
    if(int(id)<0||id>=ThemeId::Count) return;
    selectedTheme=id;applyTheme();
    if(persist) {
        std::ofstream output(getExecutableDirectory()/"ui_theme.ini",std::ios::trunc);
        output<<"SeisUiTheme1 "<<int(id)<<'\n';output.flush();savedPreference=bool(output);
    }
}
// Offer the same theme collection in the home screen and workspace display settings.
bool drawThemeSelector(const char* label,float width) {
    int index=int(currentTheme());ImGui::SetNextItemWidth(width);
    const bool changed=AppUi::Widgets::Combo(label,&index,themeNames,int(ThemeId::Count));
    if(changed) setTheme(ThemeId(index));
    if(!savedPreference&&ImGui::IsItemHovered()) AppUi::Widgets::SetTooltip("Theme applied. Could not save ui_theme.ini beside the executable.");
    return changed;
}
// Reuse the selector choices in compact workspace menus.
void drawThemeMenu() {
    if(AppUi::Widgets::BeginMenu("Interface theme")) {
        for(int i=0;i<int(ThemeId::Count);++i) if(AppUi::Widgets::MenuItem(themeNames[i],nullptr,currentTheme()==ThemeId(i))) setTheme(ThemeId(i));
        ImGui::EndMenu();
    }
}
// Legacy routes draw translucent panels over dark GL canvases; retain their text contrast locally.
ThemePanelScope::ThemePanelScope() {
    ImGuiStyle desired=ImGui::GetStyle();configureThemeStyle(desired);
    for(int i=0;i<ImGuiCol_COUNT;++i) ImGui::PushStyleColor(ImGuiCol(i),desired.Colors[i]);
}
// Restore the enclosing legacy canvas colors after drawing a modern themed tool window.
ThemePanelScope::~ThemePanelScope() {ImGui::PopStyleColor(ImGuiCol_COUNT);}

LegacyContrastScope::LegacyContrastScope(bool legacyCanvas) {
    themeOnEntry=currentTheme();
    if(!legacyCanvas||!palette().light) return;
    ImGuiStyle fallback=ImGui::GetStyle();ImGui::StyleColorsDark(&fallback);
    fallback.Colors[ImGuiCol_CheckMark]=palette().accent;
    fallback.Colors[ImGuiCol_SliderGrab]=palette().accent;
    for(int i=0;i<ImGuiCol_COUNT;++i) ImGui::PushStyleColor(ImGuiCol(i),fallback.Colors[i]);
    colorCount=ImGuiCol_COUNT;
}
// Restore the outer application theme without changing its persisted selection.
LegacyContrastScope::~LegacyContrastScope() {
    if(colorCount) ImGui::PopStyleColor(colorCount);
    if(currentTheme()!=themeOnEntry) applyTheme();
}
} // namespace AppUi
