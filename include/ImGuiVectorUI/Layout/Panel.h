// ImGuiVectorUI — floating, collapsible menu panel (ported from MYGui::NewWindow).
#pragma once

#include "imgui.h"
#include <functional>

namespace IVUI
{
    // Optional title-bar controls + drag-drop for managed/composable panels.
    struct PanelHeader
    {
        bool  showEdit = false;  // draw a pencil (edit-mode) button
        bool  showClose = false; // draw an "x" button
        bool  editActive = false;// highlight the pencil (this panel is being edited)

        // Double-click-to-rename: provide a buffer and the title becomes editable.
        char* editBuf = nullptr;
        int   editBufSize = 0;

        // outputs (set each frame):
        bool  editClicked = false;
        bool  closeClicked = false;
        bool  nameChanged = false;             // editBuf holds the new name this frame
        const char* droppedFeature = nullptr;  // a feature id dropped onto the panel, or null
    };

    // Draws a titled surface at `pos`. Right-click the title bar collapses it to
    // just the bar; body height animates in/out. `maxSize` bounds the expanded panel.
    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, ImGuiWindowFlags flags = 0);

    // Managed variant with title-bar buttons + a feature drop target.
    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, PanelHeader* header, ImGuiWindowFlags flags = 0);
}
