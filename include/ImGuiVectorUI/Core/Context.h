/** @file Context.h
 *  @brief Context lifetime and host integration for ImGuiVectorUI.
 */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/AnimState.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include <vector>
namespace IVUI
{
    struct Context
    {
        AnimRegistry       anim;
        std::vector<Theme> themeStack;
        Theme              themeTarget;               ///<  morph target (set each frame)
        Theme              themeLive;                 ///<  eased toward target; published to stack base
        bool               themeInit = false;         ///<  themeLive seeded yet?
        float              deltaTime = 1.0f / 60.0f; ///<  clamped real frame time
        unsigned           frameCount = 0;
        ImFont*            fontRegular = nullptr;
        ImFont*            fontTitle = nullptr;
        bool               wantCaptureHotkey = false; ///<  a HotkeyInput is capturing this frame
        float              contentScale = 1.0f;       ///<  DPI scale from the host
    };

    /** @brief Creates the library context after the ImGui backend is initialized. */
    void     CreateContext();
    /** @brief Releases all library-owned state before destroying the ImGui context. */
    void     DestroyContext();
    /** @brief Advances library state once per frame, immediately after ImGui::NewFrame(). */
    void     NewFrame();
    /** @brief Returns the active IVUI context.
     *  @return The active context, or null before CreateContext() and after DestroyContext().
     */
    Context* GetContext();

    /** @brief Sets library fonts; null pointers use ImGui's default font.
     *  @param regular Font used for regular UI text.
     *  @param title Font used for title text.
     */
    void     SetFonts(ImFont* regular, ImFont* title);

    /** @brief Reports whether a HotkeyInput owns keyboard capture this frame. */
    bool     WantCaptureHotkey();

    /** @brief Sets the host DPI scale used to preserve the UI's apparent size.
     *  @param scale Positive scale relative to the design resolution.
     */
    void     SetContentScale(float scale);
    /** @return The DPI scale last supplied by SetContentScale(). */
    float    GetContentScale();
}
