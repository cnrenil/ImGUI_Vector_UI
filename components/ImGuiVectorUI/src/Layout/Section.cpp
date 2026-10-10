#include "ImGuiVectorUI/Layout/Section.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Layout/AutoSizeChild.h"

#include "imgui_internal.h"

namespace IVUI
{
    bool Section(const char* label, bool* open, const std::function<void()>& body)
    {
        const Theme& th = GetTheme();
        AnimSlot& a = Slot(label);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float S = th.Scale;
        const float headerH = th.ControlHeight * S;
        // Keep the header clear of a possible overlay scrollbar.
        const float avail = ImMax(ImGui::GetContentRegionAvail().x - 8.0f * S, 40.0f * S);

        const bool clicked = ImGui::InvisibleButton(label, ImVec2(avail, headerH),
                                                    ImGuiButtonFlags_EnableNav);
        if (clicked)
            *open = !*open;
        const ImVec2 hmin = ImGui::GetItemRectMin();
        const ImVec2 hmax = ImGui::GetItemRectMax();

        a.direction2 = *open ? 1.0f : -1.0f;
        a.t2 = AdvanceProgress(a.t2, *open ? 1.0f : 0.0f, 6.0f);
        const float open01 = a.t2;
        const float rnd = th.RoundingControl * S;
        const bool fullFill = (th.Selection == SEL_FullFill);
        const ImU32 hdrCol = fullFill
            ? EaseValueColor(ImColor(th.Surface1), ImColor(th.Accent), open01, EASE_IN_OUT_CIRC)
            : EaseValueColor(ImColor(th.Surface1), ImColor(th.Surface2), open01, EASE_IN_OUT_CIRC);
        const ImU32 txtCol = fullFill
            ? EaseValueColor(ImColor(th.Text), ImColor(th.OnAccent), open01, EASE_IN_OUT_CIRC)
            : EaseValueColor(ImColor(th.Text), ImColor(th.Accent), open01, EASE_IN_OUT_CIRC);
        draw->AddRectFilled(hmin, hmax, WithAlphaMul(hdrCol, th.SurfaceAlpha), rnd);
        if (th.Selection == SEL_Underline)
            draw->AddLine(ImVec2(hmin.x + rnd, hmax.y - 1.5f * S), ImVec2(hmax.x - rnd, hmax.y - 1.5f * S),
                          WithAlphaMul(th.Accent, open01), ImMax(2.0f * S, 1.0f));
        else if (th.Selection == SEL_LeftBar)
            draw->AddRectFilled(hmin, ImVec2(hmin.x + 3.0f * S, hmax.y), WithAlphaMul(th.Accent, open01), rnd,
                                ImDrawFlags_RoundCornersLeft);
        else if (th.Selection == SEL_Outline)
            draw->AddRect(hmin, hmax, WithAlphaMul(th.Accent, open01), rnd, 0, ImMax(th.BorderThickness * S, 1.0f));
        Sheen(draw, hmin, hmax, rnd, th.Sheen);

        const ImVec2 labelSize = CalcTextScaled(label, th.FontSizeLabel);
        AddTextScaled(draw, ImVec2(hmin.x + 12.0f * S, hmin.y + (headerH - labelSize.y) * 0.5f), txtCol, label, th.FontSizeLabel);

        const ImVec2 chevC = ImVec2(hmax.x - 16.0f * S, hmin.y + headerH * 0.5f);
        IconChevron(draw, chevC, 14.0f * S, txtCol, ImMax(1.6f * S, 1.0f), *open ? 1 : 0);

        a.direction = *open ? 1.0f : -1.0f;
        a.t = AdvanceProgress(a.t, *open ? 1.0f : 0.0f, 5.0f);
        if (a.t > 0.001f)
        {
            const float visibleH = EaseValue(0.0f, a.sizeY, a.t, EASE_OUT_CUBIC);
            ImGui::Dummy(ImVec2(0.0f, 4.0f * S));
            const float measured = AnimatedChild(label, ImVec2(avail, visibleH), 0.0f, body);
            a.sizeY = measured;
        }
        return clicked;
    }
}
