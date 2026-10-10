#pragma once

#include "ImGuiVectorUI/Theme/Theme.h"

namespace IVUI
{
    struct ThemePalette
    {
        ImU32 accent = IM_COL32(82, 207, 160, 255);
        ImU32 onAccent = IM_COL32(12, 39, 29, 255);
        ImU32 surface = IM_COL32(23, 24, 26, 255);
        ImU32 surfaceRaised = IM_COL32(29, 31, 33, 255);
        ImU32 control = IM_COL32(40, 43, 46, 255);
        ImU32 text = IM_COL32(235, 238, 240, 255);
        ImU32 textMuted = IM_COL32(172, 181, 188, 255);
        ImU32 textDisabled = IM_COL32(111, 119, 126, 255);
        ImU32 border = IM_COL32(70, 77, 82, 255);
        ImU32 borderMuted = IM_COL32(49, 54, 59, 255);
        ImU32 hover = IM_COL32(120, 196, 255, 22);
        ImU32 success = IM_COL32(82, 207, 160, 255);
        ImU32 warning = IM_COL32(241, 196, 86, 255);
        ImU32 error = IM_COL32(248, 117, 126, 255);
        ImU32 info = IM_COL32(112, 183, 255, 255);

        static ThemePalette Dark();
        static ThemePalette Light();
    };

    struct ThemeMetrics
    {
        float controlHeight = 38.0f;
        float itemSpacing = 9.0f;
        ImVec2 panelPadding{18, 16};
        ImVec2 controlPadding{10, 7};
        float titleSize = 20.0f;
        float labelSize = 16.0f;
        float smallSize = 13.0f;
        ImVec2 toggleSize{40, 24};

        static ThemeMetrics Comfortable();
        static ThemeMetrics Compact();
    };

    struct ThemeShape
    {
        float panelRounding = 8.0f;
        float controlRounding = 5.0f;
        float borderThickness = 1.0f;
    };

    /** Source of truth for a custom theme. Generated tokens may be overridden
     *  afterwards for specialized effects or individual element roles.
     */
    struct ThemeDefinition
    {
        ThemePalette palette{};
        ThemeMetrics metrics{};
        ThemeShape shape{};
        ItemMotionStyle motion = ItemMotionStyle::FadeSlide({0, 4}, 0.16f);
        float scale = 1.0f;
    };

    Theme MakeTheme(const ThemeDefinition& definition);
}
