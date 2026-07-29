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

    void Track(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding)
    {
        draw->AddRectFilled(min, max, col, rounding);
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
