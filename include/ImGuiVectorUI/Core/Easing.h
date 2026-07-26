// ImGuiVectorUI — Easing engine.
// Ported from ImGui-UI1001 (OpaiUI) MainUI/Easing. Pure math over ImGui types,
// zero backend dependency.
#pragma once

#include "imgui.h"

namespace IVUI
{
    enum EasingType
    {
        EASE_LINEAR,
        EASE_IN_QUAD,
        EASE_OUT_QUAD,
        EASE_IN_OUT_QUAD,
        EASE_IN_CUBIC,
        EASE_OUT_CUBIC,
        EASE_IN_OUT_CUBIC,
        EASE_IN_QUART,
        EASE_OUT_QUART,
        EASE_IN_OUT_QUART,
        EASE_IN_SINE,
        EASE_OUT_SINE,
        EASE_IN_OUT_SINE,
        EASE_IN_EXPO,
        EASE_OUT_EXPO,
        EASE_IN_OUT_EXPO,
        EASE_IN_CIRC,
        EASE_OUT_CIRC,
        EASE_IN_OUT_CIRC,
        EASE_OUT_ELASTIC,
    };

    float   EaseValue(float start, float end, float t, EasingType type);
    float   EaseValueCalc(float start, float end, float t, EasingType type);
    bool    EaseValueColor(ImColor start, ImColor end, ImColor* v, float t, EasingType type);
    ImColor EaseValueColor(ImColor start, ImColor end, float t, EasingType type);
    bool    EaseValueImVec2(ImVec2 start, ImVec2 end, ImVec2* v, float t, EasingType type);
    ImVec2  EaseValueImVec2(ImVec2 start, ImVec2 end, float t, EasingType type);
}
