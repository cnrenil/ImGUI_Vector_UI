// ImGuiVectorUI — renderer-agnostic GPU-effect bridge.
//
// Widgets/panels can request a "glass" backdrop (translucent tint + outer glow +
// drop shadow + edge sheen) drawn by a custom GPU shader *behind* the ImGui draw
// data. The library only records screen-space rects + theme params into a per-frame
// queue; the host's DX12 (or other) backend consumes the queue and renders it in a
// dedicated pass before ImGui. This keeps IVUI itself free of any GPU/backend code.
#pragma once

#include "imgui.h"

namespace IVUI
{
    // One glass panel to composite. Positions are in screen (pixel) space.
    struct GlassRect
    {
        ImVec2 min, max;      // rect in screen pixels
        float  rounding;      // corner radius in pixels
        float  opacity;       // 0..1 fill strength of the glass tint
        ImU32  tint;          // glass body color (alpha ignored; opacity drives it)
        ImU32  accent;        // edge-sheen / accent color
        ImU32  glow;          // outer-glow color (alpha ignored; glowStrength drives it)
        float  glowStrength;  // 0..1 outer glow intensity
        float  glowSpread;    // px falloff distance of the outer glow
        float  shadowStrength;// 0..1 drop-shadow intensity
        float  shadowSpread;  // px falloff distance of the drop shadow
    };

    // Global glass style shared by library panels (IVUI::Panel) and tunable from
    // the host/demo. `bodyAlpha` is the opacity of the ImGui-drawn panel fill on
    // top of the GPU glass (lower = more see-through). Spreads are in unscaled px
    // (multiplied by the theme Scale at submit time).
    struct GlassStyle
    {
        bool  enabled = true;
        float opacity = 0.55f;        // GPU glass tint fill strength
        float bodyAlpha = 0.72f;      // ImGui panel-fill opacity over the glass
        float glowStrength = 0.55f;
        float glowSpread = 26.0f;
        float shadowStrength = 0.5f;
        float shadowSpread = 18.0f;
    };
    void              SetGlassStyle(const GlassStyle& s);
    const GlassStyle& GetGlassStyle();

    // Queue a glass backdrop for this frame (call while building the UI, e.g. from
    // inside a Panel). Cleared automatically at the next IVUI::NewFrame().
    void SubmitGlass(const GlassRect& r);

    // Host hook: read the glass queue for this frame. Returns the array base and
    // writes the count. Valid until the next NewFrame(). Never null when count > 0.
    const GlassRect* CollectGlass(int* outCount);
}
