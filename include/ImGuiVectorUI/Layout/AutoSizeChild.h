/// ImGuiVectorUI — the "measure lambda content, animate height" helper, extracted
/// once from MYGui's three container widgets.
#pragma once

#include "imgui.h"
#include <functional>

namespace IVUI
{
    /** @brief Renders auto-sized content clipped to an outer animated child.
     *  @param id Unique child identifier.
     *  @param outerSize Visible child size; its height is normally animated by the caller.
     *  @param innerWidth Width available to @p body during measurement.
     *  @param body Content callback.
     *  @return Unclipped measured content height.
     */
    float AnimatedChild(const char* id, ImVec2 outerSize, float innerWidth, const std::function<void()>& body);
}
