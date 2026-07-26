// ImGuiVectorUI — thin drag-and-drop helpers carrying a feature id string.
#pragma once

namespace IVUI
{
    // Call right after an item (e.g. a drag grip) to make it a drag source that
    // carries `featureId`. `displayName` is shown as the drag ghost. Returns true
    // while dragging.
    bool BeginFeatureDragSource(const char* featureId, const char* displayName);

    // Make the LAST submitted item a drop target; returns the dropped feature id
    // (valid until next frame) or nullptr.
    const char* AcceptFeatureDrop();

    // Make an explicit screen rect a drop target (for whole panels/windows).
    const char* AcceptFeatureDropOnRect(const void* imguiId, float x0, float y0, float x1, float y1);

    // Is a feature drag currently in flight? (for highlighting drop targets)
    bool IsFeatureDragActive();

    // Draw a themed "chip" as the drag preview (call between Begin/EndDragDropSource).
    void DrawDragGhost(const char* name);
}
