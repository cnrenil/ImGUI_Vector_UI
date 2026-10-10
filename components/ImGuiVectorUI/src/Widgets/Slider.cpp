#include "ImGuiVectorUI/Widgets/Slider.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui_internal.h"

#include <cstdio>
#include <cmath>
#include <utility>

namespace IVUI
{
    static ImVec2 SliderChrome(const char* label, const char* valueText, AnimSlot& /*a*/,
                                ImVec2& trackSize, bool& clicked, float& mousePercent,
                                bool& dragging, int& navStep)
    {
        const Theme& th = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;

        trackSize = ImVec2(ImMax(avail.x - 12.0f * S, 40.0f * S), th.SliderTrackH * S);

        const ImVec2 labelSize = CalcTextScaled(label, th.FontSizeSmall);
        AddTextScaled(draw, cursor, th.TextMuted, label, th.FontSizeSmall);
        const ImVec2 valSize = CalcTextScaled(valueText, th.FontSizeSmall);
        AddTextScaled(draw, ImVec2(cursor.x + trackSize.x - valSize.x, cursor.y), th.TextMuted, valueText, th.FontSizeSmall);

        ImGui::SetCursorScreenPos(cursor);
        clicked = ImGui::InvisibleButton(label, ImVec2(trackSize.x, trackSize.y + labelSize.y + 12.0f * S),
                                         ImGuiButtonFlags_EnableNav);
        dragging = ImGui::IsItemActive() && ImGui::IsMouseDown(0);
        navStep = 0;
        if (ImGui::IsItemFocused())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) navStep = -1;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) navStep = 1;
        }
        mousePercent = ImClamp((ImGui::GetMousePos().x - cursor.x) / trackSize.x, 0.0f, 1.0f);

        return ImVec2(cursor.x, cursor.y + labelSize.y + 6.0f * S);
    }

    static void DrawTrack(ImVec2 bgPos, ImVec2 size, float percent)
    {
        const Theme& th = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float rounding = PillRounding(size.y);
        draw->AddRectFilled(bgPos, bgPos + size, th.AccentMuted, rounding);
        const float w = size.x * percent;
        draw->AddRectFilled(bgPos, bgPos + ImVec2(w, size.y), th.Accent, rounding);
        const ImVec2 kc(bgPos.x + w, bgPos.y + size.y * 0.5f);
        const float kr = th.KnobRadius * th.Scale;
        KnobShape(draw, kc, kr, th.Knob, th.KnobRoundFrac);
    }

    bool SliderFloat(const char* label, float* v, float mn, float mx, const char* fmt)
    {
        if (!v) return false;
        if (mx < mn) std::swap(mx, mn);
        *v = ImClamp(*v, mn, mx);
        AnimSlot& a = Slot(label);
        char buf[256];
        snprintf(buf, sizeof(buf), fmt, *v);

        ImVec2 size; bool clicked, dragging; float mp; int navStep;
        const ImVec2 bgPos = SliderChrome(label, buf, a, size, clicked, mp, dragging, navStep);
        const float previous = *v;
        if (dragging)
            *v = mn + mp * (mx - mn);
        if (navStep != 0)
            *v = ImClamp(*v + (mx - mn) * 0.01f * navStep, mn, mx);

        const float percent = (mx > mn) ? (*v - mn) / (mx - mn) : 0.0f;
        if (fabsf(percent - a.lastTarget) > 1e-4f) { a.lastTarget = percent; a.t = 0.0f; }
        a.t = AdvanceProgress(a.t, 1.0f, 8.0f);
        a.currentPercent = EaseValue(a.currentPercent, percent, a.t, EASE_LINEAR);

        DrawTrack(bgPos, size, a.currentPercent);
        return *v != previous;
    }

    int SliderSegments(const char* label, int* v, int segments, const char* fmt)
    {
        if (!v || segments <= 0) return 0;
        *v = ImClamp(*v, 0, segments - 1);
        AnimSlot& a = Slot(label);
        const Theme& th = GetTheme();
        char buf[256];
        snprintf(buf, sizeof(buf), fmt, *v);

        ImVec2 size; bool clicked, dragging; float mp; int navStep;
        const ImVec2 bgPos = SliderChrome(label, buf, a, size, clicked, mp, dragging, navStep);
        if ((dragging || clicked) && segments > 0)
        {
            int nv = (int)(mp * segments);
            *v = ImClamp(nv, 0, segments - 1);
        }
        if (navStep != 0) *v = ImClamp(*v + navStep, 0, segments - 1);

        const float percent = (segments > 1) ? (float)(*v) / (segments - 1) : 0.0f;
        if (fabsf(percent - a.lastTarget) > 1e-4f) { a.lastTarget = percent; a.t = 0.0f; }
        a.t = AdvanceProgress(a.t, 1.0f, 8.0f);
        a.currentPercent = EaseValue(a.currentPercent, percent, a.t, EASE_LINEAR);

        DrawTrack(bgPos, size, a.currentPercent);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (segments > 1)
            for (int i = 1; i < segments; ++i)
            {
                const float x = bgPos.x + size.x * i / (segments - 1);
                const ImU32 c = (i <= *v) ? th.Surface0 : th.Accent;
                draw->AddCircleFilled(ImVec2(x, bgPos.y + size.y * 0.5f), 1.5f * th.Scale, c);
            }
        return *v;
    }

    int SliderLabeled(const char* label, int* v, std::initializer_list<const char*> opts, const char* fmt)
    {
        if (!v) return 0;
        AnimSlot& a = Slot(label);
        const Theme& th = GetTheme();
        const int segments = (int)opts.size();
        if (segments <= 0)
            return 0;
        *v = ImClamp(*v, 0, segments - 1);

        char buf[256];
        snprintf(buf, sizeof(buf), fmt, *(opts.begin() + *v));

        ImVec2 size; bool clicked, dragging; float mp; int navStep;
        const ImVec2 bgPos = SliderChrome(label, buf, a, size, clicked, mp, dragging, navStep);
        if (dragging || clicked)
            *v = ImClamp((int)(mp * (segments - 1) + 0.5f), 0, segments - 1);
        if (navStep != 0) *v = ImClamp(*v + navStep, 0, segments - 1);

        const float target = (float)(*v);
        if (fabsf(a.currentValue - target) > 0.01f)
        {
            a.currentValue += (target - a.currentValue) * MotionResponse(12.0f);
            if (fabsf(a.currentValue - target) < 0.01f)
                a.currentValue = target;
        }
        else
        {
            a.currentValue = target;
        }

        const float percent = (segments > 1) ? a.currentValue / (segments - 1) : 0.0f;
        DrawTrack(bgPos, size, percent);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (segments > 1)
            for (int i = 1; i < segments; ++i)
            {
                const float x = bgPos.x + size.x * i / (segments - 1);
                const ImU32 c = (i <= (int)(a.currentValue + 0.5f)) ? th.Surface0 : th.Accent;
                draw->AddCircleFilled(ImVec2(x, bgPos.y + size.y * 0.5f), 1.5f * th.Scale, c);
            }
        return *v;
    }
}
