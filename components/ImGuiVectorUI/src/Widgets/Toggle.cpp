#include "ImGuiVectorUI/Widgets/Toggle.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui_internal.h" // ImClamp, ImMax

namespace IVUI
{
    bool Toggle(const char* label, bool* v)
    {
        const Theme& th = GetTheme();
        AnimSlot& a = Slot(label);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;

        const ImVec2 bg = ImVec2(th.ToggleW * S, th.ToggleH * S);

        a.direction = *v ? 1.0f : -1.0f;
        a.t = AdvanceProgress(a.t, a.direction > 0.0f ? 1.0f : 0.0f, 8.0f);

        const ImVec2 labelSize = CalcTextScaled(label, th.FontSizeLabel);
        AddTextScaled(draw, ImVec2(cursor.x, cursor.y + (bg.y - labelSize.y) * 0.5f), th.Text, label, th.FontSizeLabel);

        const float rightInset = 12.0f * S;
        const ImVec2 bgPos = ImVec2(cursor.x + ImMax(avail.x - rightInset - bg.x, bg.x), cursor.y);
        const float rounding = PillRounding(bg.y);
        const ImU32 track = EaseValueColor(ImColor(th.TrackOff), ImColor(th.Accent), a.t, EASE_IN_OUT_CIRC);
        draw->AddRectFilled(bgPos, bgPos + bg, track, rounding);
        Sheen(draw, bgPos, bgPos + bg, rounding, th.Sheen);

        const float knobR = EaseValue(bg.y * 0.32f, bg.y * 0.38f, a.t, EASE_IN_OUT_CIRC);
        const ImVec2 knobC = ImVec2(bgPos.x + EaseValue(bg.y * 0.5f, bg.x - bg.y * 0.5f, a.t, EASE_IN_OUT_CIRC),
                                    bgPos.y + bg.y * 0.5f);
        const ImU32 knob = EaseValueColor(ImColor(th.TrackOff), ImColor(th.Knob), a.t, EASE_IN_OUT_CIRC);
        KnobShape(draw, knobC, knobR, knob, th.KnobRoundFrac);
        const ImU32 check = EaseValueColor(ImColor(0, 0, 0, 0), ImColor(th.OnAccent), a.t, EASE_IN_OUT_CIRC);
        IconCheck(draw, knobC, knobR * 1.15f, check, ImMax(1.5f * S, 1.0f));

        ImGui::SetCursorScreenPos(cursor);
        const bool clicked = ImGui::InvisibleButton(label, ImVec2(ImMax(avail.x, bg.x), bg.y),
                                                    ImGuiButtonFlags_EnableNav);
        if (clicked)
            *v = !*v;
        return clicked;
    }
}
