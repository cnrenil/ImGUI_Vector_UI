/** @file ScrollRegion.h @brief Auto-hiding overlay scrollbar. */
#pragma once

namespace IVUI
{
    /** @brief Draws an overlay scrollbar for the current child window.
     *  @pre The child uses ImGuiWindowFlags_NoScrollbar and permits mouse-wheel scrolling.
     *  Call immediately before EndChild().
     */
    void OverlayScrollbar();
}
