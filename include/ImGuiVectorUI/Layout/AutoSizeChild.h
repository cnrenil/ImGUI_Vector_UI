// ImGuiVectorUI — the "measure lambda content, animate height" helper, extracted
// once from MYGui's three container widgets.
#pragma once

#include "imgui.h"
#include <functional>

namespace IVUI
{
    // Renders `body` inside a nested auto-resize child clipped to `outerSize`.
    // Returns the measured (unclipped) content height so the caller can animate
    // `outerSize.y` toward it next frame.
    float AnimatedChild(const char* id, ImVec2 outerSize, float innerWidth, const std::function<void()>& body);
}
