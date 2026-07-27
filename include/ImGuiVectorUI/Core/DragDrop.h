/** @file DragDrop.h @brief Feature-ID drag-and-drop helpers. */
#pragma once

namespace IVUI
{
    /** @brief Starts a drag source for the last item.
     *  @param featureId Stable identifier placed in the payload.
     *  @param displayName Text shown in the drag preview.
     *  @return True while the source is being dragged.
     */
    bool BeginFeatureDragSource(const char* featureId, const char* displayName);

    /** @brief Accepts a feature drop on the last item.
     *  @return An ID valid until the next frame, or null when nothing was dropped.
     */
    const char* AcceptFeatureDrop();

    /** @brief Accepts a feature drop on an explicit screen-space rectangle.
     *  @param imguiId Unique ImGui ID for the target.
     *  @param x0,y0 Upper-left screen position.
     *  @param x1,y1 Lower-right screen position.
     *  @return An ID valid until the next frame, or null when nothing was dropped.
     */
    const char* AcceptFeatureDropOnRect(const void* imguiId, float x0, float y0, float x1, float y1);

    /** @brief Returns whether a feature drag is active. */
    bool IsFeatureDragActive();

    /** @brief Draws the drag preview between ImGui drag-source begin/end calls. */
    void DrawDragGhost(const char* name);
}
