// ImGuiVectorUI — auto-hiding overlay scrollbar.
#pragma once

namespace IVUI
{
    // Draws an auto-hiding, draggable overlay scrollbar for the CURRENT child
    // window. Call it once just before EndChild.
    //
    // The child must be created with ImGuiWindowFlags_NoScrollbar (but WITHOUT
    // NoScrollWithMouse, so the wheel still scrolls). That keeps the content
    // region width constant whether or not scrolling is active — the bar overlays
    // the right gutter instead of insetting content (which would jitter widths).
    void OverlayScrollbar();
}
