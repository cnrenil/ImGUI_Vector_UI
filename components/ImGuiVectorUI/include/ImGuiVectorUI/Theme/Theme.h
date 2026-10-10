/// ImGuiVectorUI — theme token system.
/// Widgets never hardcode colors/sizes; they read semantic tokens from the
/// current theme. Themes can be pushed/popped and mutated at runtime — because
/// widgets ease *toward* token values each frame, changing a token animates the
/// whole UI into the new palette for free.
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Widgets/Notification.h"

#include <array>
#include <cstddef>

namespace IVUI
{
    enum class ElementRole : std::size_t
    {
        Default,
        Panel,
        Button,
        ProminentButton,
        Toggle,
        Slider,
        Input,
        Text,
        Notification,
        Chart,
        ColorPicker,
        ModelViewer,
        Count
    };

    struct ElementVisualStyle
    {
        ItemEffectRequest effects{};
        ItemMotionStyle motion{};
    };

    struct Theme
    {
        /// Semantic colors (ImU32, packed RGBA).
        ImU32 Accent;
        ImU32 AccentHover;
        ImU32 AccentActive;
        ImU32 AccentMuted;   ///<  accent at low alpha (slider track background, etc.)
        ImU32 OnAccent;      ///<  text/icon drawn on top of an accent fill

        ImU32 Surface0;      ///<  outermost panel background
        ImU32 Surface1;      ///<  group / section background
        ImU32 Surface2;      ///<  control background (combo header, toggle-off track)
        ImU32 Overlay;       ///<  translucent scrim behind the floating panel

        ImU32 Border;
        ImU32 BorderMuted;

        ImU32 Text;
        ImU32 TextMuted;
        ImU32 TextDisabled;

        ImU32 TrackOff;      ///<  toggle knob (off)
        ImU32 TrackOn;       ///<  toggle knob (on)
        ImU32 Knob;          ///<  slider knob

        ImU32 HoverTint;     ///<  additive tint drawn on hovered rows/controls
        ImU32 GlassTint;     ///<  independent tint mixed into the blurred panel backdrop
        ImU32 Success = IM_COL32(64, 200, 120, 255);
        ImU32 Warning = IM_COL32(240, 190, 70, 255);
        ImU32 Error = IM_COL32(235, 90, 90, 255);
        ImU32 Info = IM_COL32(112, 183, 255, 255);

        /// Shape scale.
        float RoundingPanel;
        float RoundingControl;
        float RoundingPill;
        float BorderThickness;

        /// Spacing / sizing (unscaled; multiply by Scale at the draw site).
        ImVec2 PanelPadding;
        ImVec2 ControlPadding;
        float  ItemSpacing;
        float  ControlHeight;
        float  ToggleW;
        float  ToggleH;
        float  SliderTrackH;
        float  KnobRadius;

        /// Typography (point sizes at Scale == 1).
        float   FontSizeTitle;
        float   FontSizeLabel;
        float   FontSizeSmall;
        ImFont* FontRegular; ///<  nullptr -> fall back to the context/default font
        ImFont* FontTitle;

        /// ---- Style parameters (a "style" is a whole visual language, not just a
        /// palette). Continuous ones are eased each frame by the morph layer so
        /// switching style animates; discrete ones snap. ----
        /// Continuous (eased):
        float WidgetGlow;        ///<  per-widget glow strength (0 = none)
        float WidgetGlowSpread;  ///<  px falloff of per-widget glow (unscaled)
        float ControlCornerFrac; ///<  pill controls' corner radius = height * frac (0=boxy .. 0.5=pill)
        float KnobRoundFrac;     ///<  knob corner radius fraction (1=circle .. 0=square)
        float SurfaceAlpha;      ///<  control fill opacity (1=opaque, <1=translucent)
        float PanelBodyAlpha;    ///<  ImGui-drawn panel/header overlay opacity over GPU glass
        float GlassTintStrength; ///<  0 = pure blur, 1 = fully tinted glass body
        float GlassOpacity;      ///<  final glass layer opacity over the blurred backdrop
        float GlassRefraction;   ///<  px of normal-directed rim displacement (0 = no rim refraction)
        float GlassEdgeWidth;    ///<  px width of the refractive material rim (unscaled)
        float GlassEdgeHighlight;///<  0..1 strength of the directional specular rim
        float GlassThickness;    ///<  0..1 optical depth; increases tint, refraction and shadow
        float GlassShapePower;   ///<  rounded-edge lens response exponent (higher = a tighter refractive rim)
        float GlassLensStrength; ///<  exponential center-directed backdrop compression amplitude
        float GlassLensBase;     ///<  base of the lens exponential curve
        float GlassLensFalloff;  ///<  distance decay of the lens exponential curve
        float GlassLensBias;     ///<  rim bias of the lens exponential curve
        float GlassLensPower;    ///<  final power applied to the lens scale
        float GlassNoise;        ///<  px of subtle time-varying refraction noise
        float Sheen;             ///<  top specular highlight strength (0..1)
        float PanelGlow;         ///<  final framebuffer bloom strength
        float PanelGlowSpread;   ///<  panel glow px falloff (unscaled)
        float BloomThreshold;    ///<  minimum average RGB brightness included in bloom
        float BloomSaturation;   ///<  minimum RGB chroma included in bloom
        float BloomSigma;        ///<  Gaussian sigma in pixels for the bloom blur
        float BackdropBlurSigma; ///<  Gaussian sigma in pixels for panel backdrops (0 = disabled)
        float PanelShadow;       ///<  panel drop-shadow strength
        float PanelShadowSpread; ///<  panel shadow px falloff (unscaled)
        float BackdropAlpha;     ///<  multiplier for the host-wide scrim behind the menu
        /// Active-feature HUD geometry and effects (all unscaled).
        float HudMinWidth;
        float HudPadding;
        float HudRowGap;
        float HudPanelAlpha;
        float HudRowAlpha;
        float HudBorderThickness;
        float HudRainbowSpeed;
        float HudRainbowGlow;
        /// Discrete (snap on style change):
        int   Selection;         ///<  SelectionStyle: how selected rows/sections read
        int   HudLayout;         ///<  HudLayout: active-feature HUD composition
        bool  FillGradient;      ///<  classic-ImGui vertical gradient control fills

        /// Notification composition. Individual notifications may override every field.
        NotificationTheme Notifications{};

        /// Per-element defaults. Widgets may override either value per instance.
        std::array<ElementVisualStyle, static_cast<std::size_t>(ElementRole::Count)> Elements{};

        const ElementVisualStyle& ForElement(ElementRole role) const noexcept
        {
            const std::size_t index = static_cast<std::size_t>(role);
            return Elements[index < Elements.size() ? index : 0];
        }

        ElementVisualStyle& ForElement(ElementRole role) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(role);
            return Elements[index < Elements.size() ? index : 0];
        }

        ElementVisualStyle ResolveElement(ElementRole role) const noexcept
        {
            ElementVisualStyle resolved = ForElement(role);
            resolved.motion.offset.x *= Scale;
            resolved.motion.offset.y *= Scale;
            for (std::uint32_t index = 0; index < resolved.effects.layerCount; ++index)
            {
                resolved.effects.layers[index].offset.x *= Scale;
                resolved.effects.layers[index].offset.y *= Scale;
                resolved.effects.layers[index].sigma *= Scale;
            }
            return resolved;
        }

        /// Global multiplier (supersedes MYGui's SCALE() macro).
        float Scale;
    };

    /// How a selected/active row or header is emphasized.
    enum SelectionStyle { SEL_FullFill = 0, SEL_Underline, SEL_LeftBar, SEL_Outline };

    /// Active-feature HUD layouts. Geometry and effects remain theme parameters.
    enum HudLayout { HUD_Chips = 0, HUD_SideRail, HUD_RainbowFrame, HUD_GlassStack };

    enum class Preset { Peach, Cyan, Violet, Emerald, COUNT };
    enum class Style { MaterialYou, Vape, Neverlose, ImGuiClassic, iOSGlass, COUNT };

    enum class ColorScheme { Dark, Light };
    enum class Density { Comfortable, Compact };

    /** Standalone theme factory; no UI or ImGui context is required. */
    Theme MakeTheme(ColorScheme scheme = ColorScheme::Dark,
                    Density density = Density::Comfortable);
    /** Map semantic tokens onto native ImGui controls without cumulative scaling. */
    void ApplyImGuiTheme(const Theme& theme, ImGuiStyle& style);

#ifdef IVUI_INTERNAL_API
    /** @name Built-in color presets */
    /** @{ */
    Theme ThemeInjectionPeach();   ///<  default — the UI1001 salmon/charcoal look
    Theme ThemeInjectionCyan();    ///<  the current-repo cyan look
    Theme ThemeInjectionViolet();
    Theme ThemeInjectionEmerald();
    /** @} */

    /** @brief Creates the color preset selected by @p p. */
    Theme       GetPreset(Preset p);
    /** @return The display name for @p p. */
    const char* PresetName(Preset p);

    /// Global visual styles — a whole look (colors + shape + glow + translucency +
    /// motion feel), not just a palette. Built on top of the color presets.
    /** @brief Creates the complete visual style selected by @p s. */
    Theme       GetStyle(Style s);
    /** @return The display name for @p s. */
    const char* StyleName(Style s);
    /** @brief Recomputes accent-derived theme tokens.
     *  @param t Theme to update.
     *  @param accent Base packed RGBA accent color.
     */
    void SetAccent(Theme& t, ImU32 accent);

    /** @brief Returns a faded copy of @p t.
     *  Color alpha values and intensity-based visual effects are scaled by @p a.
     */
    Theme ScaleThemeAlpha(const Theme& t, float a);

    /** @brief Pushes @p theme onto the active context's theme stack. */
    void         PushTheme(const Theme& theme);
    /** @brief Removes the top theme from the active context's stack. */
    void         PopTheme();
    /** @return The read-only theme at the top of the active stack. */
    const Theme& GetTheme();
    /** @return The mutable theme at the top of the active stack. */
    Theme&       GetMutableTheme();

    /** @brief Sets the desired theme for frame-by-frame morphing.
     *  @param t Target theme. Call once per frame while the desired look is active.
     */
    void SetTargetTheme(const Theme& t);
    /** @brief Advances the live theme toward its target.
     *  @param dt Elapsed time in seconds.
     */
    void AdvanceThemeMorph(float dt);
#endif
}
