#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Core/Context.h"

#include "imgui_internal.h" // ImLerp

#include <algorithm>
#include <cmath> // expf
#include <stdexcept>

namespace IVUI
{
    static ImU32 LerpCol(ImU32 a, ImU32 b, float t)
    {
        return ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
    }
    static ImU32 WithAlpha(ImU32 c, int a)
    {
        return (c & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
    }

    static EffectColor ToEffectColor(ImU32 color)
    {
        const ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        return {value.x, value.y, value.z, value.w};
    }

    static void FillElementStyles(Theme& t, Style style)
    {
        ItemMotionStyle motion = ItemMotionStyle::FadeSlide();
        switch (style)
        {
        case Style::Vape:
            motion = ItemMotionStyle::FadeSlide({10.0f, 0.0f}, 0.16f);
            motion.scale = 1.0f;
            break;
        case Style::Neverlose:
            motion = ItemMotionStyle::FadeSlide({8.0f, 0.0f}, 0.14f);
            motion.scale = 1.0f;
            break;
        case Style::ImGuiClassic:
            motion = ItemMotionStyle::Fade(0.08f);
            motion.animateInitial = false;
            break;
        case Style::iOSGlass:
            motion = ItemMotionStyle::FadeSlide({0.0f, 10.0f}, 0.24f);
            motion.enterEasing = EASE_OUT_CIRC;
            motion.exitEasing = EASE_IN_CIRC;
            motion.scale = 0.96f;
            break;
        default:
            break;
        }

        for (ElementVisualStyle& element : t.Elements)
        {
            element.effects = {};
            element.motion = motion;
        }

        const EffectColor accent = ToEffectColor(t.Accent);
        const float sigma = (std::max)(t.WidgetGlowSpread * 0.5f, 0.5f);
        t.ForElement(ElementRole::Button).effects.Add(
            ItemEffectLayer::Glow(accent, sigma, 0.75f));
        t.ForElement(ElementRole::ProminentButton).effects.Add(
            ItemEffectLayer::Glow(accent, sigma, 1.15f));
        t.ForElement(ElementRole::Toggle).effects.Add(
            ItemEffectLayer::Glow(accent, sigma * 0.75f, 0.65f));
        t.ForElement(ElementRole::Panel).effects.Add(ItemEffectLayer::Shadow(
            {0.0f, 0.0f, 0.0f, 0.8f}, {0.0f, 6.0f},
            (std::max)(t.PanelShadowSpread * 0.5f, 0.5f), t.PanelShadow));
        t.ForElement(ElementRole::Notification).effects.Add(ItemEffectLayer::Shadow(
            {0.0f, 0.0f, 0.0f, 0.72f}, {0.0f, 5.0f}, 7.0f, 0.45f));
        t.ForElement(ElementRole::Text).motion = ItemMotionStyle::Fade(0.18f);
    }

    Theme ScaleThemeAlpha(const Theme& src, float a)
    {
        a = (a < 0.0f) ? 0.0f : (a > 1.0f ? 1.0f : a);
        Theme t = src;
        ImU32* cols[] = {
            &t.Accent, &t.AccentHover, &t.AccentActive, &t.AccentMuted, &t.OnAccent,
            &t.Surface0, &t.Surface1, &t.Surface2, &t.Overlay, &t.Border, &t.BorderMuted,
            &t.Text, &t.TextMuted, &t.TextDisabled, &t.TrackOff, &t.TrackOn, &t.Knob,
            &t.HoverTint, &t.GlassTint, &t.Success, &t.Warning, &t.Error, &t.Info
        };
        for (ImU32* c : cols)
        {
            const int alpha = (int)(((*c >> IM_COL32_A_SHIFT) & 0xFF) * a);
            *c = (*c & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
        }
        // GPU glass and other intensity-based effects do not consume color alpha.
        t.SurfaceAlpha *= a;
        t.WidgetGlow *= a;
        t.Sheen *= a;
        t.PanelBodyAlpha *= a;
        t.GlassOpacity *= a;
        t.PanelGlow *= a;
        t.PanelShadow *= a;
        t.BackdropAlpha *= a;
        t.HudPanelAlpha *= a;
        t.HudRowAlpha *= a;
        t.HudRainbowGlow *= a;
        for (ElementVisualStyle& element : t.Elements)
            for (std::uint32_t index = 0; index < element.effects.layerCount; ++index)
                element.effects.layers[index].opacity *= a;
        return t;
    }

    void SetAccent(Theme& t, ImU32 accent)
    {
        t.Accent = WithAlpha(accent, 255);
        t.AccentHover = LerpCol(t.Accent, IM_COL32_WHITE, 0.20f);
        t.AccentActive = LerpCol(t.Accent, IM_COL32_BLACK, 0.15f);
        t.AccentMuted = WithAlpha(t.Accent, 80);
        t.Knob = t.Accent;
        t.TrackOn = LerpCol(t.Accent, IM_COL32_BLACK, 0.35f);
        for (ElementVisualStyle& element : t.Elements)
            for (std::uint32_t index = 0; index < element.effects.layerCount; ++index)
                if (element.effects.layers[index].type == ItemEffectType::Glow)
                    element.effects.layers[index].color = ToEffectColor(t.Accent);
    }

    static void FillCommonMetrics(Theme& t)
    {
        t.RoundingPanel = 10.0f;
        t.RoundingControl = 6.0f;
        t.RoundingPill = 22.0f;
        t.BorderThickness = 1.5f;

        t.PanelPadding = ImVec2(14.0f, 12.0f);
        t.ControlPadding = ImVec2(10.0f, 8.0f);
        t.ItemSpacing = 8.0f;
        t.ControlHeight = 40.0f;
        t.ToggleW = 42.0f;
        t.ToggleH = 26.0f;
        t.SliderTrackH = 5.0f;
        t.KnobRadius = 6.0f;

        t.FontSizeTitle = 22.0f;
        t.FontSizeLabel = 18.0f;
        t.FontSizeSmall = 15.0f;
        t.FontRegular = nullptr;
        t.FontTitle = nullptr;

        t.WidgetGlow = 0.0f;
        t.WidgetGlowSpread = 10.0f;
        t.ControlCornerFrac = 0.5f;
        t.KnobRoundFrac = 1.0f;
        t.SurfaceAlpha = 1.0f;
        t.PanelBodyAlpha = 0.72f;
        t.GlassTint = t.Surface0;
        t.GlassTintStrength = 0.16f;
        t.GlassOpacity = 0.82f;
        t.GlassRefraction = 1.25f;
        t.GlassEdgeWidth = 2.0f;
        t.GlassEdgeHighlight = 0.22f;
        t.GlassThickness = 0.35f;
        t.GlassShapePower = 4.0f;
        t.GlassLensStrength = 0.30f;
        t.GlassLensBase = 5.2f;
        t.GlassLensFalloff = 6.9f;
        t.GlassLensBias = 0.7f;
        t.GlassLensPower = 1.0f;
        t.GlassNoise = 0.0f;
        t.Sheen = 0.0f;
        t.PanelGlow = 0.55f;
        t.PanelGlowSpread = 26.0f;
        t.BloomThreshold = 0.08f;
        t.BloomSaturation = 0.04f;
        t.BloomSigma = 3.0f;
        t.BackdropBlurSigma = 16.0f;
        t.PanelShadow = 0.5f;
        t.PanelShadowSpread = 18.0f;
        t.BackdropAlpha = 1.0f;
        t.HudMinWidth = 124.0f;
        t.HudPadding = 10.0f;
        t.HudRowGap = 5.0f;
        t.HudPanelAlpha = 0.92f;
        t.HudRowAlpha = 0.72f;
        t.HudBorderThickness = 1.0f;
        t.HudRainbowSpeed = 0.18f;
        t.HudRainbowGlow = 0.24f;
        t.Selection = SEL_FullFill;
        t.HudLayout = HUD_Chips;
        t.FillGradient = false;
        t.Notifications = {};
        if (t.HoverTint == 0) // default only if a preset didn't set it
            t.HoverTint = IM_COL32(255, 255, 255, 14);

        t.Scale = 1.0f;
        FillElementStyles(t, Style::MaterialYou);
    }

    Theme ThemeInjectionPeach()
    {
        Theme t{};
        t.Accent = IM_COL32(255, 180, 169, 255);
        t.AccentHover = IM_COL32(255, 198, 188, 255);
        t.AccentActive = IM_COL32(255, 160, 146, 255);
        t.AccentMuted = IM_COL32(251, 180, 169, 80);
        t.OnAccent = IM_COL32(60, 30, 25, 255);

        t.Surface0 = IM_COL32(28, 30, 34, 255);
        t.Surface1 = IM_COL32(36, 38, 43, 255);
        t.Surface2 = IM_COL32(51, 52, 57, 255);
        t.Overlay = IM_COL32(10, 11, 14, 150);

        t.Border = IM_COL32(70, 72, 78, 255);
        t.BorderMuted = IM_COL32(54, 53, 60, 255);

        t.Text = IM_COL32(233, 231, 232, 255);
        t.TextMuted = IM_COL32(205, 207, 193, 255);
        t.TextDisabled = IM_COL32(130, 132, 138, 255);

        t.TrackOff = IM_COL32(118, 118, 120, 255);
        t.TrackOn = IM_COL32(128, 86, 75, 255);
        t.Knob = IM_COL32(251, 180, 169, 255);

        FillCommonMetrics(t);
        return t;
    }

    Theme ThemeInjectionCyan()
    {
        Theme t{};
        t.Accent = IM_COL32(51, 199, 250, 255);
        t.AccentHover = IM_COL32(90, 214, 255, 255);
        t.AccentActive = IM_COL32(30, 170, 220, 255);
        t.AccentMuted = IM_COL32(51, 199, 250, 70);
        t.OnAccent = IM_COL32(8, 16, 22, 255);

        t.Surface0 = IM_COL32(16, 18, 26, 255);
        t.Surface1 = IM_COL32(23, 26, 36, 255);
        t.Surface2 = IM_COL32(32, 36, 48, 255);
        t.Overlay = IM_COL32(6, 8, 12, 150);

        t.Border = IM_COL32(48, 54, 70, 255);
        t.BorderMuted = IM_COL32(36, 40, 52, 255);

        t.Text = IM_COL32(224, 232, 240, 255);
        t.TextMuted = IM_COL32(150, 170, 190, 255);
        t.TextDisabled = IM_COL32(90, 100, 115, 255);

        t.TrackOff = IM_COL32(70, 78, 92, 255);
        t.TrackOn = IM_COL32(24, 120, 150, 255);
        t.Knob = IM_COL32(51, 199, 250, 255);

        FillCommonMetrics(t);
        return t;
    }

    Theme ThemeInjectionViolet()
    {
        Theme t = ThemeInjectionPeach();
        SetAccent(t, IM_COL32(179, 136, 255, 255));
        t.OnAccent = IM_COL32(24, 12, 44, 255);
        t.TrackOff = IM_COL32(92, 88, 112, 255);
        return t;
    }

    Theme ThemeInjectionEmerald()
    {
        Theme t = ThemeInjectionPeach();
        SetAccent(t, IM_COL32(52, 211, 153, 255));
        t.OnAccent = IM_COL32(6, 32, 24, 255);
        t.TrackOff = IM_COL32(80, 100, 92, 255);
        return t;
    }

    Theme GetPreset(Preset p)
    {
        switch (p)
        {
        case Preset::Cyan:    return ThemeInjectionCyan();
        case Preset::Violet:  return ThemeInjectionViolet();
        case Preset::Emerald: return ThemeInjectionEmerald();
        default:              return ThemeInjectionPeach();
        }
    }

    const char* PresetName(Preset p)
    {
        switch (p)
        {
        case Preset::Cyan:    return "Cyan";
        case Preset::Violet:  return "Violet";
        case Preset::Emerald: return "Emerald";
        default:              return "Peach";
        }
    }

    Theme GetStyle(Style s)
    {
        switch (s)
        {
        default:
        case Style::MaterialYou:
        {
            Theme t = ThemeInjectionPeach();
            t.RoundingPanel = 14.0f;
            t.RoundingControl = 11.0f;
            t.ControlCornerFrac = 0.42f;
            t.BorderThickness = 0.0f;    // elevation via shadow, not borders
            t.WidgetGlow = 0.12f;        // subtle — keeps prominent buttons alive
            t.SurfaceAlpha = 1.0f;
            t.Sheen = 0.05f;
            t.PanelGlow = 0.35f;
            t.PanelShadow = 0.65f;
            t.PanelShadowSpread = 20.0f;
            t.Selection = SEL_FullFill;
            t.KnobRoundFrac = 1.0f;
            FillElementStyles(t, Style::MaterialYou);
            return t;
        }
        case Style::Vape:
        {
            Theme t{};
            t.Surface0 = IM_COL32(11, 10, 14, 255);
            t.Surface1 = IM_COL32(17, 16, 21, 255);
            t.Surface2 = IM_COL32(29, 27, 35, 255);
            t.Overlay = IM_COL32(4, 4, 7, 200);
            t.Border = IM_COL32(255, 255, 255, 27);
            t.BorderMuted = IM_COL32(255, 255, 255, 15);
            t.Text = IM_COL32(243, 241, 248, 255);
            t.TextMuted = IM_COL32(165, 158, 177, 255);
            t.TextDisabled = IM_COL32(92, 87, 102, 255);
            t.OnAccent = IM_COL32(20, 16, 29, 255);
            t.TrackOff = IM_COL32(53, 49, 62, 255);
            SetAccent(t, IM_COL32(174, 139, 255, 255));
            t.HoverTint = IM_COL32(174, 139, 255, 20);
            FillCommonMetrics(t);
            t.RoundingPanel = 3.0f;
            t.RoundingControl = 2.0f;
            t.ControlCornerFrac = 0.10f;
            t.BorderThickness = 1.0f;
            t.ControlHeight = 36.0f;
            t.ItemSpacing = 6.0f;
            t.WidgetGlow = 0.28f;
            t.WidgetGlowSpread = 9.0f;
            t.KnobRoundFrac = 0.20f;
            t.Selection = SEL_LeftBar;
            t.HudLayout = HUD_SideRail;
            t.HudMinWidth = 132.0f;
            t.HudPadding = 9.0f;
            t.HudRowGap = 2.0f;
            t.HudRowAlpha = 0.72f;
            t.HudBorderThickness = 2.0f;
            t.PanelGlow = 0.16f;
            t.BloomThreshold = 0.16f;
            t.BloomSaturation = 0.10f;
            t.BloomSigma = 2.2f;
            t.BackdropBlurSigma = 9.0f;
            t.GlassTint = IM_COL32(28, 20, 42, 255);
            t.GlassTintStrength = 0.24f;
            t.GlassOpacity = 0.80f;
            t.PanelBodyAlpha = 0.48f;
            t.PanelShadow = 0.72f;
            t.PanelShadowSpread = 22.0f;
            t.BackdropAlpha = 0.90f;
            FillElementStyles(t, Style::Vape);
            return t;
        }
        case Style::Neverlose:
        {
            Theme t{};
            t.Surface0 = IM_COL32(12, 14, 17, 255);
            t.Surface1 = IM_COL32(18, 21, 25, 255);
            t.Surface2 = IM_COL32(28, 33, 39, 255);
            t.Overlay = IM_COL32(4, 6, 8, 190);
            t.Border = IM_COL32(57, 72, 89, 255);
            t.BorderMuted = IM_COL32(36, 46, 57, 255);
            t.Text = IM_COL32(232, 237, 243, 255);
            t.TextMuted = IM_COL32(148, 163, 179, 255);
            t.TextDisabled = IM_COL32(82, 94, 108, 255);
            t.OnAccent = IM_COL32(7, 17, 27, 255);
            t.TrackOff = IM_COL32(58, 69, 82, 255);
            SetAccent(t, IM_COL32(76, 167, 255, 255));
            t.HoverTint = IM_COL32(76, 167, 255, 20);
            FillCommonMetrics(t);
            t.RoundingPanel = 2.0f;
            t.RoundingControl = 2.0f;
            t.ControlCornerFrac = 0.08f;
            t.BorderThickness = 1.0f;
            t.ControlHeight = 36.0f;
            t.ItemSpacing = 6.0f;
            t.WidgetGlow = 0.38f;
            t.WidgetGlowSpread = 10.0f;
            t.KnobRoundFrac = 0.10f;
            t.Selection = SEL_LeftBar;
            t.HudLayout = HUD_RainbowFrame;
            t.HudMinWidth = 150.0f;
            t.HudPadding = 7.0f;
            t.HudRowGap = 0.0f;
            t.HudPanelAlpha = 0.94f;
            t.HudRainbowSpeed = 0.18f;
            t.HudRainbowGlow = 0.24f;
            t.HudBorderThickness = 1.5f;
            t.PanelGlow = 0.24f;
            t.BloomThreshold = 0.12f;
            t.BloomSaturation = 0.08f;
            t.BloomSigma = 2.6f;
            t.BackdropBlurSigma = 12.0f;
            t.GlassTint = IM_COL32(18, 35, 53, 255);
            t.GlassTintStrength = 0.20f;
            t.GlassOpacity = 0.78f;
            t.PanelBodyAlpha = 0.52f;
            t.PanelShadow = 0.64f;
            t.PanelShadowSpread = 20.0f;
            t.BackdropAlpha = 0.82f;
            FillElementStyles(t, Style::Neverlose);
            return t;
        }
        case Style::ImGuiClassic:
        {
            Theme t{};
            t.Surface0 = IM_COL32(21, 21, 24, 255);
            t.Surface1 = IM_COL32(29, 30, 34, 255);
            t.Surface2 = IM_COL32(41, 51, 71, 255);   // bluish frame like classic FrameBg
            t.Overlay = IM_COL32(10, 10, 12, 150);
            t.Border = IM_COL32(78, 82, 92, 255);
            t.BorderMuted = IM_COL32(58, 60, 68, 255);
            t.Text = IM_COL32(235, 235, 235, 255);
            t.TextMuted = IM_COL32(180, 182, 186, 255);
            t.TextDisabled = IM_COL32(128, 130, 134, 255);
            t.OnAccent = IM_COL32(255, 255, 255, 255);
            t.TrackOff = IM_COL32(70, 74, 84, 255);
            SetAccent(t, IM_COL32(66, 150, 250, 255)); // classic ImGui blue
            t.HoverTint = IM_COL32(66, 150, 250, 40);
            FillCommonMetrics(t);
            t.RoundingPanel = 0.0f;
            t.RoundingControl = 0.0f;
            t.RoundingPill = 0.0f;
            t.ControlCornerFrac = 0.0f;
            t.BorderThickness = 1.0f;
            t.WidgetGlow = 0.0f;
            t.FillGradient = true;
            t.KnobRoundFrac = 0.0f;      // square grabs
            t.Selection = SEL_FullFill;
            t.Sheen = 0.0f;
            t.PanelGlow = 0.0f;
            t.PanelShadow = 0.0f;
            t.PanelBodyAlpha = 1.0f;
            t.GlassTint = t.Surface0;
            t.GlassTintStrength = 0.96f;
            t.GlassOpacity = 1.0f;
            t.BloomThreshold = 0.50f;
            t.BloomSaturation = 0.20f;
            t.BloomSigma = 1.0f;
            t.BackdropBlurSigma = 0.0f;
            FillElementStyles(t, Style::ImGuiClassic);
            return t;
        }
        case Style::iOSGlass:
        {
            Theme t{};
            t.Surface0 = IM_COL32(118, 136, 164, 255);
            t.Surface1 = IM_COL32(151, 168, 194, 255);
            t.Surface2 = IM_COL32(190, 205, 227, 255);
            t.Overlay = IM_COL32(10, 18, 33, 105);
            t.Border = IM_COL32(255, 255, 255, 145);
            t.BorderMuted = IM_COL32(255, 255, 255, 72);
            t.Text = IM_COL32(255, 255, 255, 255);
            t.TextMuted = IM_COL32(227, 235, 246, 255);
            t.TextDisabled = IM_COL32(183, 196, 215, 255);
            t.OnAccent = IM_COL32(255, 255, 255, 255);
            t.TrackOff = IM_COL32(158, 175, 198, 255);
            SetAccent(t, IM_COL32(10, 132, 255, 255)); // iOS blue
            t.HoverTint = IM_COL32(255, 255, 255, 58);
            FillCommonMetrics(t);
            t.RoundingPanel = 24.0f;
            t.RoundingControl = 15.0f;
            t.RoundingPill = 40.0f;
            t.ControlCornerFrac = 0.5f;   // full pills
            t.BorderThickness = 1.0f;     // hairline (color already low alpha)
            t.ToggleW = 50.0f;
            t.ToggleH = 30.0f;
            t.WidgetGlow = 0.18f;
            t.WidgetGlowSpread = 14.0f;
            t.SurfaceAlpha = 0.42f;       // translucent glass
            t.PanelBodyAlpha = 0.27f;
            t.GlassTint = IM_COL32(178, 207, 238, 255);
            t.GlassTintStrength = 0.28f;
            t.GlassOpacity = 0.68f;
            t.GlassRefraction = 3.2f;
            t.GlassEdgeWidth = 3.0f;
            t.GlassEdgeHighlight = 0.44f;
            t.GlassThickness = 0.72f;
            t.GlassShapePower = 4.6f;
            t.GlassLensStrength = 2.3f;
            t.GlassLensBase = 5.2f;
            t.GlassLensFalloff = 6.9f;
            t.GlassLensBias = 0.7f;
            t.GlassLensPower = 1.0f;
            t.GlassNoise = 0.35f;
            t.BloomThreshold = 0.18f;
            t.BloomSaturation = 0.03f;
            t.BloomSigma = 3.0f;
            t.BackdropBlurSigma = 16.0f;
            t.Sheen = 0.95f;              // bright liquid-glass rim
            t.KnobRoundFrac = 1.0f;
            t.Selection = SEL_FullFill;
            t.HudLayout = HUD_GlassStack;
            t.HudMinWidth = 124.0f;
            t.HudPadding = 13.0f;
            t.HudRowGap = 5.0f;
            t.HudPanelAlpha = 0.55f;
            t.HudRowAlpha = 0.85f;
            t.HudBorderThickness = 1.0f;
            t.PanelGlow = 0.18f;
            t.PanelShadow = 0.52f;
            t.PanelShadowSpread = 30.0f;
            t.BackdropAlpha = 0.34f;
            t.Notifications.style = NotificationStyle::DynamicIsland;
            t.Notifications.placement = NotificationPlacement::TopCenter;
            t.Notifications.width = 0.0f;
            t.Notifications.maxWidth = 460.0f;
            t.Notifications.duration = 5.0f;
            t.Notifications.showProgress = false;
            t.Notifications.offset = ImVec2(0.0f, 4.0f);
            FillElementStyles(t, Style::iOSGlass);
            ElementVisualStyle& notification = t.ForElement(ElementRole::Notification);
            notification.motion = ItemMotionStyle::FadeSlide({0.0f, -12.0f}, 0.28f);
            notification.motion.scale = 0.72f;
            notification.motion.enterEasing = EASE_OUT_CIRC;
            notification.motion.exitEasing = EASE_IN_CIRC;
            return t;
        }
        }
    }

    const char* StyleName(Style s)
    {
        switch (s)
        {
        case Style::Vape:         return "VAPE";
        case Style::Neverlose:    return "Neverlose";
        case Style::ImGuiClassic: return "ImGui Classic";
        case Style::iOSGlass:     return "iOS Liquid Glass";
        default:                  return "Material You";
        }
    }


    static const Theme g_fallback = MakeTheme();

    static void MorphColor(ImU32& c, ImU32 target, float k) { c = LerpCol(c, target, k); }
    static void MorphF(float& v, float target, float k) { v += (target - v) * k; }

    static void MorphElementStyle(ElementVisualStyle& live,
                                  const ElementVisualStyle& target, float k)
    {
        bool compatible = live.effects.layerCount == target.effects.layerCount;
        for (std::uint32_t index = 0; compatible && index < live.effects.layerCount; ++index)
            compatible = live.effects.layers[index].type == target.effects.layers[index].type;
        if (!compatible)
        {
            bool faded = true;
            for (std::uint32_t index = 0; index < live.effects.layerCount; ++index)
            {
                MorphF(live.effects.layers[index].opacity, 0.0f, k);
                faded = faded && live.effects.layers[index].opacity < 0.01f;
            }
            if (faded)
            {
                live.effects = target.effects;
                for (std::uint32_t index = 0; index < live.effects.layerCount; ++index)
                    live.effects.layers[index].opacity = 0.0f;
            }
        }
        else
        {
            for (std::uint32_t index = 0; index < live.effects.layerCount; ++index)
            {
                ItemEffectLayer& layer = live.effects.layers[index];
                const ItemEffectLayer& destination = target.effects.layers[index];
                MorphF(layer.offset.x, destination.offset.x, k);
                MorphF(layer.offset.y, destination.offset.y, k);
                MorphF(layer.color.r, destination.color.r, k);
                MorphF(layer.color.g, destination.color.g, k);
                MorphF(layer.color.b, destination.color.b, k);
                MorphF(layer.color.a, destination.color.a, k);
                MorphF(layer.sigma, destination.sigma, k);
                MorphF(layer.strength, destination.strength, k);
                MorphF(layer.opacity, destination.opacity, k);
                layer.flags = destination.flags;
            }
        }

        MorphF(live.motion.enterDuration, target.motion.enterDuration, k);
        MorphF(live.motion.exitDuration, target.motion.exitDuration, k);
        MorphF(live.motion.offset.x, target.motion.offset.x, k);
        MorphF(live.motion.offset.y, target.motion.offset.y, k);
        MorphF(live.motion.scale, target.motion.scale, k);
        MorphF(live.motion.minimumAlpha, target.motion.minimumAlpha, k);
        live.motion.enterEasing = target.motion.enterEasing;
        live.motion.exitEasing = target.motion.exitEasing;
        live.motion.animateInitial = target.motion.animateInitial;
    }

    void SetTargetTheme(const Theme& t)
    {
        Context* c = GetContext();
        if (!c)
            return;
        c->themeTarget = t;
        if (!c->themeInit)
        {
            c->themeLive = t;       // first assignment: no startup jump
            c->themeInit = true;
        }
    }

    void AdvanceThemeMorph(float dt)
    {
        Context* c = GetContext();
        if (!c || !c->themeInit)
            return;

        Theme& L = c->themeLive;
        const Theme& T = c->themeTarget;
        const float k = c->motion.reducedMotion ? 1.0f : -std::expm1(-12.0f * dt);

        ImU32* lc[] = { &L.Accent,&L.AccentHover,&L.AccentActive,&L.AccentMuted,&L.OnAccent,
            &L.Surface0,&L.Surface1,&L.Surface2,&L.Overlay,&L.Border,&L.BorderMuted,
            &L.Text,&L.TextMuted,&L.TextDisabled,&L.TrackOff,&L.TrackOn,&L.Knob,&L.HoverTint,&L.GlassTint,
            &L.Success,&L.Warning,&L.Error,&L.Info };
        const ImU32* tc[] = { &T.Accent,&T.AccentHover,&T.AccentActive,&T.AccentMuted,&T.OnAccent,
            &T.Surface0,&T.Surface1,&T.Surface2,&T.Overlay,&T.Border,&T.BorderMuted,
            &T.Text,&T.TextMuted,&T.TextDisabled,&T.TrackOff,&T.TrackOn,&T.Knob,&T.HoverTint,&T.GlassTint,
            &T.Success,&T.Warning,&T.Error,&T.Info };
        for (int i = 0; i < (int)(sizeof(lc) / sizeof(lc[0])); ++i)
            MorphColor(*lc[i], *tc[i], k);

        MorphF(L.RoundingPanel, T.RoundingPanel, k);
        MorphF(L.RoundingControl, T.RoundingControl, k);
        MorphF(L.RoundingPill, T.RoundingPill, k);
        MorphF(L.BorderThickness, T.BorderThickness, k);
        MorphF(L.PanelPadding.x, T.PanelPadding.x, k);
        MorphF(L.PanelPadding.y, T.PanelPadding.y, k);
        MorphF(L.ControlPadding.x, T.ControlPadding.x, k);
        MorphF(L.ControlPadding.y, T.ControlPadding.y, k);
        MorphF(L.ItemSpacing, T.ItemSpacing, k);
        MorphF(L.ControlHeight, T.ControlHeight, k);
        MorphF(L.ToggleW, T.ToggleW, k);
        MorphF(L.ToggleH, T.ToggleH, k);
        MorphF(L.SliderTrackH, T.SliderTrackH, k);
        MorphF(L.KnobRadius, T.KnobRadius, k);
        MorphF(L.WidgetGlow, T.WidgetGlow, k);
        MorphF(L.WidgetGlowSpread, T.WidgetGlowSpread, k);
        MorphF(L.ControlCornerFrac, T.ControlCornerFrac, k);
        MorphF(L.KnobRoundFrac, T.KnobRoundFrac, k);
        MorphF(L.SurfaceAlpha, T.SurfaceAlpha, k);
        MorphF(L.PanelBodyAlpha, T.PanelBodyAlpha, k);
        MorphF(L.GlassTintStrength, T.GlassTintStrength, k);
        MorphF(L.GlassOpacity, T.GlassOpacity, k);
        MorphF(L.GlassRefraction, T.GlassRefraction, k);
        MorphF(L.GlassEdgeWidth, T.GlassEdgeWidth, k);
        MorphF(L.GlassEdgeHighlight, T.GlassEdgeHighlight, k);
        MorphF(L.GlassThickness, T.GlassThickness, k);
        MorphF(L.GlassShapePower, T.GlassShapePower, k);
        MorphF(L.GlassLensStrength, T.GlassLensStrength, k);
        MorphF(L.GlassLensBase, T.GlassLensBase, k);
        MorphF(L.GlassLensFalloff, T.GlassLensFalloff, k);
        MorphF(L.GlassLensBias, T.GlassLensBias, k);
        MorphF(L.GlassLensPower, T.GlassLensPower, k);
        MorphF(L.GlassNoise, T.GlassNoise, k);
        MorphF(L.Sheen, T.Sheen, k);
        MorphF(L.PanelGlow, T.PanelGlow, k);
        MorphF(L.PanelGlowSpread, T.PanelGlowSpread, k);
        MorphF(L.BloomThreshold, T.BloomThreshold, k);
        MorphF(L.BloomSaturation, T.BloomSaturation, k);
        MorphF(L.BloomSigma, T.BloomSigma, k);
        MorphF(L.BackdropBlurSigma, T.BackdropBlurSigma, k);
        MorphF(L.PanelShadow, T.PanelShadow, k);
        MorphF(L.PanelShadowSpread, T.PanelShadowSpread, k);
        MorphF(L.BackdropAlpha, T.BackdropAlpha, k);
        MorphF(L.HudMinWidth, T.HudMinWidth, k);
        MorphF(L.HudPadding, T.HudPadding, k);
        MorphF(L.HudRowGap, T.HudRowGap, k);
        MorphF(L.HudPanelAlpha, T.HudPanelAlpha, k);
        MorphF(L.HudRowAlpha, T.HudRowAlpha, k);
        MorphF(L.HudBorderThickness, T.HudBorderThickness, k);
        MorphF(L.HudRainbowSpeed, T.HudRainbowSpeed, k);
        MorphF(L.HudRainbowGlow, T.HudRainbowGlow, k);
        MorphF(L.Notifications.viewportPadding.x, T.Notifications.viewportPadding.x, k);
        MorphF(L.Notifications.viewportPadding.y, T.Notifications.viewportPadding.y, k);
        MorphF(L.Notifications.offset.x, T.Notifications.offset.x, k);
        MorphF(L.Notifications.offset.y, T.Notifications.offset.y, k);
        MorphF(L.Notifications.width, T.Notifications.width, k);
        MorphF(L.Notifications.maxWidth, T.Notifications.maxWidth, k);
        MorphF(L.Notifications.spacing, T.Notifications.spacing, k);
        MorphF(L.Notifications.duration, T.Notifications.duration, k);
        for (std::size_t index = 0; index < L.Elements.size(); ++index)
            MorphElementStyle(L.Elements[index], T.Elements[index], k);

        // Font changes require atlas rebuilds; discrete visual modes must not interpolate.
        L.Scale = T.Scale;
        L.FontSizeTitle = T.FontSizeTitle;
        L.FontSizeLabel = T.FontSizeLabel;
        L.FontSizeSmall = T.FontSizeSmall;
        L.FontRegular = T.FontRegular;
        L.FontTitle = T.FontTitle;
        L.Selection = T.Selection;
        L.HudLayout = T.HudLayout;
        L.FillGradient = T.FillGradient;
        L.Notifications.style = T.Notifications.style;
        L.Notifications.placement = T.Notifications.placement;
        L.Notifications.pauseOnHover = T.Notifications.pauseOnHover;
        L.Notifications.dismissOnClick = T.Notifications.dismissOnClick;
        L.Notifications.showProgress = T.Notifications.showProgress;
        L.Notifications.clampToViewport = T.Notifications.clampToViewport;
        L.Notifications.deduplicate = T.Notifications.deduplicate;
        L.Notifications.queueLimit = T.Notifications.queueLimit;

        if (!c->themeStack.empty())
            c->themeStack.front() = L;
    }

    void PushTheme(const Theme& theme)
    {
        if (Context* c = GetContext())
            c->themeStack.push_back(theme);
    }

    void PopTheme()
    {
        if (Context* c = GetContext())
            if (c->themeStack.size() > 1)
                c->themeStack.pop_back();
    }

    const Theme& GetTheme()
    {
        Context* c = GetContext();
        if (c && !c->themeStack.empty())
            return c->themeStack.back();
        return g_fallback;
    }

    Theme& GetMutableTheme()
    {
        Context* c = GetContext();
        if (c && !c->themeStack.empty())
            return c->themeStack.back();
        throw std::logic_error("Mutable IVUI theme access requires an active UI context.");
    }
}
