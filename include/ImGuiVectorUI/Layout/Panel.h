/** @file Panel.h @brief Floating, collapsible menu panels. */
#pragma once

#include "imgui.h"
#include <functional>

namespace IVUI
{
    /** @brief Optional title-bar controls and per-frame panel interaction results. */
    struct PanelHeader
    {
        bool  showEdit = false;  ///<  draw a pencil (edit-mode) button
        bool  showClose = false; ///<  draw an "x" button
        bool  editActive = false;///<  highlight the pencil (this panel is being edited)

        /** @brief Optional buffer that enables inline title editing. */
        char* editBuf = nullptr;
        int   editBufSize = 0;

        /** @name Per-frame outputs */
        /** @{ */
        bool  editClicked = false;
        bool  closeClicked = false;
        bool  nameChanged = false;             ///<  editBuf holds the new name this frame
        const char* droppedFeature = nullptr;  ///<  a feature id dropped onto the panel, or null
        /** @} */
    };

#ifdef IVUI_INTERNAL_API
    /** @brief Draws a titled panel at @p pos with an animated collapsible body.
     *  @param label Visible title and ImGui ID source.
     *  @param pos Initial screen position.
     *  @param maxSize Maximum expanded panel size.
     *  @param body Callback that draws panel contents.
     *  @param flags Additional ImGui window flags.
     */
    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, ImGuiWindowFlags flags = 0);

    /** @brief Draws a panel with optional header controls and a feature drop target.
     *  @param header Optional per-frame interaction input and output structure.
     */
    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, PanelHeader* header, ImGuiWindowFlags flags = 0);
#endif
}
