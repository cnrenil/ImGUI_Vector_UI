#include "ImGuiVectorUI/Layout/Tabs.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui_internal.h"

namespace IVUI
{
    bool Tab(const char* label, IconType icon, int index, int* active)
    {
        const Theme& th = GetTheme();
        AnimSlot& a = Slot(label);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float S = th.Scale;
        const float h = th.ControlHeight * S * 1.15f;
        const float w = ImGui::GetContentRegionAvail().x;

        const bool isActive = (*active == index);
        a.direction = isActive ? 1.0f : -1.0f;
        a.t = ImClamp(a.t + DeltaTime() * 8.0f * a.direction, 0.0f, 1.0f);

        const bool clicked = ImGui::InvisibleButton(label, ImVec2(w, h));
        if (clicked)
            *active = index;
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();

        const float rnd = th.RoundingControl * S;
        WidgetGlow(draw, mn, mx, th.Accent, rnd, a.t);
        const ImU32 idleBg = hovered ? th.HoverTint : IM_COL32(0, 0, 0, 0);
        const ImU32 activeBg = (th.Selection == SEL_FullFill) ? th.Surface2 : th.HoverTint;
        const ImU32 bg = EaseValueColor(ImColor(idleBg), ImColor(activeBg), a.t, EASE_OUT_QUAD);
        draw->AddRectFilled(mn, mx, bg, rnd);

        if (th.Selection == SEL_LeftBar)
        {
            const float barW = EaseValue(0.0f, 3.0f * S, a.t, EASE_OUT_QUAD);
            if (barW > 0.1f)
                draw->AddRectFilled(mn, ImVec2(mn.x + barW, mx.y), th.Accent, 1.5f * S);
        }
        else if (th.Selection == SEL_Underline)
        {
            draw->AddLine(ImVec2(mn.x + 10.0f * S, mx.y - 1.5f * S), ImVec2(mx.x - 10.0f * S, mx.y - 1.5f * S),
                          WithAlphaMul(th.Accent, a.t), ImMax(2.0f * S, 1.0f));
        }
        else if (th.Selection == SEL_Outline)
        {
            draw->AddRect(mn, mx, WithAlphaMul(th.Accent, a.t), rnd, 0, ImMax(th.BorderThickness * S, 1.0f));
        }
        else if (a.t > 0.001f)
        {
            // Filled iOS selections read as a luminous capsule, not a color block.
            draw->AddRect(mn, mx, WithAlphaMul(th.Border, a.t), rnd, 0, ImMax(th.BorderThickness * S, 1.0f));
            Sheen(draw, mn, mx, rnd, th.Sheen * a.t);
        }

        const ImU32 fg = EaseValueColor(ImColor(th.TextMuted), ImColor(th.Accent), a.t, EASE_OUT_QUAD);
        Icon(draw, icon, ImVec2(mn.x + 22.0f * S, (mn.y + mx.y) * 0.5f), 20.0f * S, fg, ImMax(1.8f * S, 1.0f));

        const ImVec2 ls = CalcTextScaled(label, th.FontSizeLabel);
        const ImU32 txt = EaseValueColor(ImColor(th.TextMuted), ImColor(th.Text), a.t, EASE_OUT_QUAD);
        AddTextScaled(draw, ImVec2(mn.x + 42.0f * S, (mn.y + mx.y) * 0.5f - ls.y * 0.5f), txt, label, th.FontSizeLabel);
        return clicked;
    }
}
