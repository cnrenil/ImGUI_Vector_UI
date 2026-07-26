#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h" // ImMin/ImMax

namespace IVUI
{
    void RectFilled(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, ImDrawFlags flags)
    {
        draw->AddRectFilled(min, max, col, rounding, flags);
    }

    void RectOutline(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float thickness, ImDrawFlags flags)
    {
        draw->AddRect(min, max, col, rounding, flags, thickness);
    }

    void Glow(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float spread, int layers)
    {
        if (layers < 1)
            layers = 1;
        const int baseA = (int)((col >> IM_COL32_A_SHIFT) & 0xFF);
        if (baseA <= 0)
            return;
        const ImU32 rgb = col & ~IM_COL32_A_MASK;
        for (int i = layers; i >= 1; --i)
        {
            const float f = (float)i / (float)layers;         // 1 outermost .. 1/layers innermost
            const float expand = spread * f;
            const int   a = (int)(baseA * (1.0f - f) * 0.5f); // fade out toward the edge
            if (a <= 0)
                continue;
            const ImU32 c = rgb | ((ImU32)a << IM_COL32_A_SHIFT);
            draw->AddRectFilled(ImVec2(min.x - expand, min.y - expand),
                                ImVec2(max.x + expand, max.y + expand),
                                c, rounding + expand);
        }
    }

    void Track(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding)
    {
        draw->AddRectFilled(min, max, col, rounding);
    }

    void WidgetGlow(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 glowColor, float rounding, float active01)
    {
        const Theme& th = GetTheme();
        const float strength = th.WidgetGlow * (active01 < 0.0f ? 0.0f : active01);
        if (strength <= 0.001f)
            return;
        const float S = th.Scale;
        // Scale the glow color's alpha by strength, then lay soft layers outward.
        const int baseA = (int)(((glowColor >> IM_COL32_A_SHIFT) & 0xFF) * ImMin(strength, 1.5f));
        const ImU32 col = (glowColor & ~IM_COL32_A_MASK) | ((ImU32)ImMin(baseA, 255) << IM_COL32_A_SHIFT);
        Glow(draw, min, max, col, rounding, th.WidgetGlowSpread * S, 5);
    }

    float PillRounding(float height)
    {
        return height * GetTheme().ControlCornerFrac;
    }

    void KnobShape(ImDrawList* draw, ImVec2 center, float radius, ImU32 col, float roundFrac)
    {
        if (roundFrac >= 0.999f)
        {
            draw->AddCircleFilled(center, radius, col);
            return;
        }
        const ImVec2 mn(center.x - radius, center.y - radius);
        const ImVec2 mx(center.x + radius, center.y + radius);
        draw->AddRectFilled(mn, mx, col, radius * roundFrac);
    }

    void Sheen(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float strength01)
    {
        if (strength01 <= 0.01f)
            return;
        const int a = (int)(strength01 * 46.0f);
        const float inset = rounding * 0.5f + 3.0f;
        draw->AddLine(ImVec2(min.x + inset, min.y + 2.0f), ImVec2(max.x - inset, min.y + 2.0f),
                      IM_COL32(255, 255, 255, a), 1.5f);
    }
}
