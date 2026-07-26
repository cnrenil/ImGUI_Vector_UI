// ImGuiVectorUI — low-level themed draw primitives on ImDrawList.
#pragma once

#include "imgui.h"

namespace IVUI
{
    void RectFilled(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, ImDrawFlags flags = 0);
    void RectOutline(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float thickness, ImDrawFlags flags = 0);

    // Soft outer glow: layered translucent rounded rects expanding outward.
    void Glow(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float spread, int layers = 6);

    // A horizontal pill track (slider background).
    void Track(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float rounding);

    // Per-widget accent glow drawn behind a control on the current draw list, gated
    // by interaction `active01` (0..1) and the theme's WidgetGlow. No-op when ~0.
    void WidgetGlow(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 glowColor, float rounding, float active01);

    // Corner radius for pill-style controls: height * theme.ControlCornerFrac
    // (already scaled if you pass a scaled height). Lets a style morph pill<->boxy.
    float PillRounding(float height);

    // Draw a knob as a rounded square: roundFrac 1 = circle, 0 = square. Morphs
    // smoothly between the two as the style's KnobRoundFrac eases.
    void KnobShape(ImDrawList* draw, ImVec2 center, float radius, ImU32 col, float roundFrac);

    // Top specular highlight inside a rounded rect (iOS/Material sheen). strength01
    // is typically the theme Sheen; no-op when ~0.
    void Sheen(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float strength01);
}
