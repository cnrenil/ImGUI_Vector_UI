#include "ImGuiVectorUI/Theme/Design.h"

#include <algorithm>
#include <cmath>

namespace IVUI
{
    ThemePalette ThemePalette::Dark() { return {}; }

    ThemePalette ThemePalette::Light()
    {
        ThemePalette p;
        p.accent = IM_COL32(0, 113, 88, 255);
        p.onAccent = IM_COL32_WHITE;
        p.surface = IM_COL32(248, 249, 250, 255);
        p.surfaceRaised = IM_COL32(239, 241, 243, 255);
        p.control = IM_COL32(225, 229, 233, 255);
        p.text = IM_COL32(27, 31, 35, 255);
        p.textMuted = IM_COL32(80, 88, 96, 255);
        p.textDisabled = IM_COL32(117, 123, 130, 255);
        p.border = IM_COL32(185, 194, 201, 255);
        p.borderMuted = IM_COL32(216, 221, 226, 255);
        p.hover = IM_COL32(0, 88, 134, 18);
        p.success = IM_COL32(0, 113, 88, 255);
        p.warning = IM_COL32(138, 94, 0, 255);
        p.error = IM_COL32(187, 45, 61, 255);
        p.info = IM_COL32(27, 100, 170, 255);
        return p;
    }

    ThemeMetrics ThemeMetrics::Comfortable() { return {}; }

    ThemeMetrics ThemeMetrics::Compact()
    {
        ThemeMetrics m;
        m.controlHeight = 30;
        m.itemSpacing = 6;
        m.panelPadding = {12, 10};
        m.controlPadding = {8, 4};
        m.labelSize = 14;
        m.smallSize = 12;
        m.toggleSize = {34, 20};
        return m;
    }

    namespace
    {
        float Metric(float value, float fallback, float minimum = 0.0f)
        {
            return std::isfinite(value) ? std::clamp(value, minimum, 512.0f) : fallback;
        }
    }

    Theme MakeTheme(const ThemeDefinition& definition)
    {
        Theme t = ThemeInjectionEmerald();
        const ThemePalette& p = definition.palette;
        const ThemeMetrics& m = definition.metrics;
        const ThemeShape& s = definition.shape;
        t.Surface0 = p.surface;
        t.Surface1 = p.surfaceRaised;
        t.Surface2 = p.control;
        t.Text = p.text;
        t.TextMuted = p.textMuted;
        t.TextDisabled = p.textDisabled;
        t.Border = p.border;
        t.BorderMuted = p.borderMuted;
        t.HoverTint = p.hover;
        t.Success = p.success;
        t.Warning = p.warning;
        t.Error = p.error;
        t.Info = p.info;
        SetAccent(t, p.accent);
        t.OnAccent = p.onAccent;
        t.TrackOff = p.textMuted;
        t.RoundingPanel = Metric(s.panelRounding, 8);
        t.RoundingControl = Metric(s.controlRounding, 5);
        t.RoundingPill = t.RoundingControl;
        t.BorderThickness = Metric(s.borderThickness, 1);
        t.ControlHeight = Metric(m.controlHeight, 38, 16);
        t.ControlCornerFrac = std::clamp(t.RoundingControl / t.ControlHeight, 0.0f, 0.5f);
        t.FontSizeTitle = Metric(m.titleSize, 20, 8);
        t.FontSizeLabel = Metric(m.labelSize, 16, 8);
        t.FontSizeSmall = Metric(m.smallSize, 13, 8);
        t.ItemSpacing = Metric(m.itemSpacing, 9);
        t.PanelPadding = {Metric(m.panelPadding.x, 18), Metric(m.panelPadding.y, 16)};
        t.ControlPadding = {Metric(m.controlPadding.x, 10), Metric(m.controlPadding.y, 7)};
        t.ToggleH = Metric(m.toggleSize.y, 24, 8);
        t.ToggleW = std::max(Metric(m.toggleSize.x, 40, 8), t.ToggleH);
        t.Scale = std::isfinite(definition.scale) && definition.scale > 0
            ? std::clamp(definition.scale, 0.25f, 8.0f) : 1.0f;
        t.Selection = SEL_LeftBar;
        t.SurfaceAlpha = t.PanelBodyAlpha = t.GlassOpacity = 1.0f;
        t.GlassTint = t.Surface0;
        t.GlassTintStrength = 1.0f;
        t.BackdropBlurSigma = t.GlassRefraction = t.GlassNoise = 0.0f;
        t.WidgetGlow = t.PanelGlow = t.Sheen = 0.0f;
        t.PanelShadow = 0.2f;
        for (auto& element : t.Elements)
        {
            element.effects = {};
            element.motion = definition.motion;
        }
        auto& notification = t.ForElement(ElementRole::Notification);
        notification.effects.Add(ItemEffectLayer::Shadow({0, 0, 0, 0.3f}, {0, 4}, 6, 0.3f));
        return t;
    }

    Theme MakeTheme(ColorScheme scheme, Density density)
    {
        ThemeDefinition definition;
        definition.palette = scheme == ColorScheme::Light ? ThemePalette::Light() : ThemePalette::Dark();
        definition.metrics = density == Density::Compact ? ThemeMetrics::Compact() : ThemeMetrics::Comfortable();
        definition.motion.scale = 1.0f;
        definition.motion.animateInitial = false;
        Theme t = MakeTheme(definition);
        t.ForElement(ElementRole::Notification).motion.animateInitial = true;
        return t;
    }

    void ApplyImGuiTheme(const Theme& t, ImGuiStyle& style)
    {
        const float scale = std::isfinite(t.Scale) && t.Scale > 0.0f ? t.Scale : 1.0f;
        style.WindowPadding = ImVec2(t.PanelPadding.x * scale, t.PanelPadding.y * scale);
        style.FramePadding = ImVec2(t.ControlPadding.x * scale, t.ControlPadding.y * scale);
        style.ItemSpacing = ImVec2(t.ItemSpacing * scale, t.ItemSpacing * scale);
        style.ItemInnerSpacing = ImVec2(6 * scale, 4 * scale);
        style.WindowRounding = t.RoundingPanel * scale;
        style.ChildRounding = style.FrameRounding = style.PopupRounding = t.RoundingControl * scale;
        style.TabRounding = t.RoundingControl * scale;
        style.WindowBorderSize = style.PopupBorderSize = t.BorderThickness * scale;
        style.FrameBorderSize = 0;
        style.ScrollbarSize = 12 * scale;
        style.GrabMinSize = 10 * scale;
        auto color = [&](ImGuiCol index, ImU32 value) { style.Colors[index] = ImGui::ColorConvertU32ToFloat4(value); };
        color(ImGuiCol_Text, t.Text);
        color(ImGuiCol_TextDisabled, t.TextDisabled);
        color(ImGuiCol_WindowBg, t.Surface0);
        color(ImGuiCol_ChildBg, t.Surface0);
        color(ImGuiCol_PopupBg, t.Surface1);
        color(ImGuiCol_Border, t.BorderMuted);
        color(ImGuiCol_FrameBg, t.Surface2);
        color(ImGuiCol_FrameBgHovered, t.BorderMuted);
        color(ImGuiCol_FrameBgActive, t.Border);
        color(ImGuiCol_TitleBg, t.Surface1);
        color(ImGuiCol_TitleBgActive, t.Surface2);
        color(ImGuiCol_MenuBarBg, t.Surface1);
        color(ImGuiCol_Button, t.Surface2);
        color(ImGuiCol_ButtonHovered, t.BorderMuted);
        color(ImGuiCol_ButtonActive, t.Border);
        color(ImGuiCol_Header, t.Surface2);
        color(ImGuiCol_HeaderHovered, t.HoverTint);
        color(ImGuiCol_HeaderActive, t.AccentMuted);
        color(ImGuiCol_CheckMark, t.Accent);
        color(ImGuiCol_SliderGrab, t.Accent);
        color(ImGuiCol_SliderGrabActive, t.AccentHover);
        color(ImGuiCol_Separator, t.BorderMuted);
        color(ImGuiCol_SeparatorHovered, t.AccentHover);
        color(ImGuiCol_SeparatorActive, t.Accent);
        color(ImGuiCol_ScrollbarBg, t.Surface1);
        color(ImGuiCol_ScrollbarGrab, t.Border);
        color(ImGuiCol_ScrollbarGrabHovered, t.TextDisabled);
        color(ImGuiCol_ScrollbarGrabActive, t.TextMuted);
        color(ImGuiCol_Tab, t.Surface1);
        color(ImGuiCol_TabHovered, t.BorderMuted);
        color(ImGuiCol_TabSelected, t.Surface2);
        color(ImGuiCol_TabSelectedOverline, t.Accent);
        color(ImGuiCol_TableHeaderBg, t.Surface1);
        color(ImGuiCol_TableBorderStrong, t.Border);
        color(ImGuiCol_TableBorderLight, t.BorderMuted);
        color(ImGuiCol_TextSelectedBg, t.AccentMuted);
        color(ImGuiCol_NavCursor, t.Accent);
    }

}
