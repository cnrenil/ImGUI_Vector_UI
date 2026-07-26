// ImGuiVectorUI — theme token system.
// Widgets never hardcode colors/sizes; they read semantic tokens from the
// current theme. Themes can be pushed/popped and mutated at runtime — because
// widgets ease *toward* token values each frame, changing a token animates the
// whole UI into the new palette for free.
#pragma once

#include "imgui.h"

namespace IVUI
{
    struct Theme
    {
        // Semantic colors (ImU32, packed RGBA).
        ImU32 Accent;
        ImU32 AccentHover;
        ImU32 AccentActive;
        ImU32 AccentMuted;   // accent at low alpha (slider track background, etc.)
        ImU32 OnAccent;      // text/icon drawn on top of an accent fill

        ImU32 Surface0;      // outermost panel background
        ImU32 Surface1;      // group / section background
        ImU32 Surface2;      // control background (combo header, toggle-off track)
        ImU32 Overlay;       // translucent scrim behind the floating panel

        ImU32 Border;
        ImU32 BorderMuted;

        ImU32 Text;
        ImU32 TextMuted;
        ImU32 TextDisabled;

        ImU32 TrackOff;      // toggle knob (off)
        ImU32 TrackOn;       // toggle knob (on)
        ImU32 Knob;          // slider knob

        ImU32 Glow;          // accent @ low alpha for bloom
        ImU32 HoverTint;     // additive tint drawn on hovered rows/controls

        // Shape scale.
        float RoundingPanel;
        float RoundingControl;
        float RoundingPill;
        float BorderThickness;

        // Spacing / sizing (unscaled; multiply by Scale at the draw site).
        ImVec2 PanelPadding;
        ImVec2 ControlPadding;
        float  ItemSpacing;
        float  ControlHeight;
        float  ToggleW;
        float  ToggleH;
        float  SliderTrackH;
        float  KnobRadius;

        // Typography (point sizes at Scale == 1).
        float   FontSizeTitle;
        float   FontSizeLabel;
        float   FontSizeSmall;
        ImFont* FontRegular; // nullptr -> fall back to the context/default font
        ImFont* FontTitle;

        // ---- Style parameters (a "style" is a whole visual language, not just a
        // palette). Continuous ones are eased each frame by the morph layer so
        // switching style animates; discrete ones snap. ----
        // Continuous (eased):
        float WidgetGlow;        // per-widget glow strength (0 = none)
        float WidgetGlowSpread;  // px falloff of per-widget glow (unscaled)
        float ControlCornerFrac; // pill controls' corner radius = height * frac (0=boxy .. 0.5=pill)
        float KnobRoundFrac;     // knob corner radius fraction (1=circle .. 0=square)
        float SurfaceAlpha;      // control fill opacity (1=opaque, <1=translucent)
        float PanelBodyAlpha;    // panel body fill opacity over the GPU glass
        float Sheen;             // top specular highlight strength (0..1)
        float PanelGlow;         // panel GPU glass glow strength
        float PanelGlowSpread;   // panel glow px falloff (unscaled)
        float PanelShadow;       // panel drop-shadow strength
        float PanelShadowSpread; // panel shadow px falloff (unscaled)
        float BackdropAlpha;     // multiplier for the host-wide scrim behind the menu
        // Active-feature HUD geometry and effects (all unscaled).
        float HudMinWidth;
        float HudPadding;
        float HudRowGap;
        float HudPanelAlpha;
        float HudRowAlpha;
        float HudBorderThickness;
        float HudRainbowSpeed;
        float HudRainbowGlow;
        // Discrete (snap on style change):
        int   Selection;         // SelectionStyle: how selected rows/sections read
        int   HudLayout;         // HudLayout: active-feature HUD composition
        bool  FillGradient;      // classic-ImGui vertical gradient control fills
        bool  DynamicIslandToast;// iOS-style top-center capsule toasts (vs bottom-right)

        // Global multiplier (supersedes MYGui's SCALE() macro).
        float Scale;
    };

    // How a selected/active row or header is emphasized.
    enum SelectionStyle { SEL_FullFill = 0, SEL_Underline, SEL_LeftBar, SEL_Outline };

    // Active-feature HUD layouts. Geometry and effects remain theme parameters.
    enum HudLayout { HUD_Chips = 0, HUD_SideRail, HUD_RainbowFrame, HUD_GlassStack };

    // Built-in presets.
    Theme ThemeInjectionPeach();   // default — the UI1001 salmon/charcoal look
    Theme ThemeInjectionCyan();    // the current-repo cyan look
    Theme ThemeInjectionViolet();
    Theme ThemeInjectionEmerald();

    enum class Preset { Peach, Cyan, Violet, Emerald, COUNT };
    Theme       GetPreset(Preset p);
    const char* PresetName(Preset p);

    // Global visual styles — a whole look (colors + shape + glow + translucency +
    // motion feel), not just a palette. Built on top of the color presets.
    enum class Style { MaterialYou, Vape, Neverlose, ImGuiClassic, iOSGlass, COUNT };
    Theme       GetStyle(Style s);
    const char* StyleName(Style s);
    bool        StyleIsDynamicIsland(Style s); // iOS-style top-center toast?

    // Recompute all accent-derived tokens (hover/active/muted/glow/knob/track) from
    // a single accent color — lets a picker or hue slider recolor the whole UI.
    void SetAccent(Theme& t, ImU32 accent);

    // Copy of `t` with every color token's alpha scaled by `a` (fade a region by
    // pushing this as the theme around it).
    Theme ScaleThemeAlpha(const Theme& t, float a);

    // Theme stack (backed by the active context).
    void         PushTheme(const Theme& theme);
    void         PopTheme();
    const Theme& GetTheme();     // read-only top of stack
    Theme&       GetMutableTheme(); // top of stack, editable (e.g. live accent swap)

    // Morph layer: set the *target* theme; each frame the live theme (what widgets
    // read via GetTheme) eases toward it, so switching style/color animates instead
    // of snapping. Call SetTargetTheme once per frame with the desired look.
    void SetTargetTheme(const Theme& t);
    void AdvanceThemeMorph(float dt); // eases live->target, writes live to stack base (called by NewFrame)
}
