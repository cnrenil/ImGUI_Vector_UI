// ImGuiVectorUI — library context and lifecycle.
// Owns the animation registry, the theme stack, per-frame timing, and font
// handles. Replaces MYGui's singleton + all its file-scope global maps.
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/AnimState.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/Effects.h"

#include <vector>

namespace IVUI
{
    struct Context
    {
        AnimRegistry       anim;
        std::vector<Theme> themeStack;
        Theme              themeTarget;               // morph target (set each frame)
        Theme              themeLive;                 // eased toward target; published to stack base
        bool               themeInit = false;         // themeLive seeded yet?
        std::vector<GlassRect> glassQueue;           // GPU glass backdrops for this frame
        float              deltaTime = 1.0f / 60.0f; // clamped real frame time
        unsigned           frameCount = 0;
        ImFont*            fontRegular = nullptr;
        ImFont*            fontTitle = nullptr;
        bool               wantCaptureHotkey = false; // a HotkeyInput is capturing this frame
        float              contentScale = 1.0f;       // DPI scale from the host
    };

    void     CreateContext();
    void     DestroyContext();
    void     NewFrame();            // call once per frame, right after ImGui::NewFrame()
    Context* GetContext();

    // Host hook: assign the fonts the library should draw with. Pass nullptr to
    // fall back to ImGui's default font.
    void     SetFonts(ImFont* regular, ImFont* title);

    // True while a HotkeyInput is waiting to capture a key. The host should use
    // this to suppress its own menu-toggle hotkey so it isn't consumed instead.
    bool     WantCaptureHotkey();

    // Host hook: report the window's DPI scale so the UI renders at native
    // resolution (crisp) while keeping its on-screen size.
    void     SetContentScale(float scale);
    float    GetContentScale();
}
