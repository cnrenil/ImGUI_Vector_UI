// ImGuiVectorUI — Motion: a tiny reusable animation library.
//
// Framerate-independent, ID-keyed tweens that any element can call in one line —
// no manual state, no per-widget timers. State lives in the context's animation
// registry and is garbage-collected automatically.
//
//   float t   = IVUI::Toggle01("mybtn", isOn);         // 0..1 eased toward isOn
//   float v   = IVUI::Approach("fov", targetValue);    // smooth-follow a number
//   ImU32 c   = IVUI::ColorApproach("hdr", target);    // eased color
//   float in  = IVUI::Transition("tab", activeTab);    // fade/slide-in on change
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Easing.h"

namespace IVUI
{
    // Ease a stored scalar toward `target` (exponential smoothing; seeded to
    // target on first use so it never jumps from 0).
    float Approach(ImGuiID id, float target, float speed = 12.0f);
    float Approach(const char* label, float target, float speed = 12.0f);

    // 0..1 progress toward a boolean state, run through an easing curve.
    float Toggle01(ImGuiID id, bool on, float speed = 8.0f, EasingType e = EASE_IN_OUT_CIRC);
    float Toggle01(const char* label, bool on, float speed = 8.0f, EasingType e = EASE_IN_OUT_CIRC);

    // Eased color toward `target` (seeded on first use).
    ImU32 ColorApproach(ImGuiID id, ImU32 target, float speed = 10.0f, EasingType e = EASE_OUT_QUAD);
    ImU32 ColorApproach(const char* label, ImU32 target, float speed = 10.0f, EasingType e = EASE_OUT_QUAD);

    // 0->1 that RESETS to 0 whenever `key` changes, then eases to 1. Use for
    // fade/slide-in transitions when a selection changes (tabs, pages).
    float Transition(ImGuiID id, int key, float speed = 6.0f, EasingType e = EASE_OUT_CUBIC);
    float Transition(const char* label, int key, float speed = 6.0f, EasingType e = EASE_OUT_CUBIC);

    // Multiply an ImU32's alpha by `a` (handy for fades).
    ImU32 WithAlphaMul(ImU32 col, float a);
}
