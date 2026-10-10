/** @file Context.h
 *  @brief Context lifetime and host integration for ImGuiVectorUI.
 */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/AnimState.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Widgets/Notification.h"
#include "ImGuiVectorUI/Widgets/ColorPicker.h"

#include <array>
#include <memory>
#include <string>
#include <vector>
namespace IVUI
{
    class IEffectRenderer;
    struct NotificationState
    {
        NotificationHandle handle{};
        std::string title;
        std::string body;
        NotificationOptions options;
        std::string actionLabel;
        std::string dedupeKey;
        float age = 0.0f;
        bool alive = true;
    };

    struct Context
    {
        AnimRegistry       anim;
        std::vector<Theme> themeStack;
        Theme              themeTarget;               ///<  morph target (set each frame)
        Theme              themeLive;                 ///<  eased toward target; published to stack base
        bool               themeInit = false;         ///<  themeLive seeded yet?
        float              deltaTime = 1.0f / 60.0f; ///<  clamped real frame time
        MotionSettings     motion{};
        double             motionTime = 0.0;
        unsigned           frameCount = 0;
        bool               frameActive = false;
        ImFont*            fontRegular = nullptr;
        ImFont*            fontTitle = nullptr;
        bool               wantCaptureHotkey = false; ///<  a HotkeyInput is capturing this frame
        float              contentScale = 1.0f;       ///<  DPI scale from the host
        IEffectRenderer*    effectRenderer = nullptr; ///< borrowed from the owning UI
        ImGuiID            keyCaptureId = 0;
        int                keyCaptureSlot = -1;
        ImGuiID            panelRenameId = 0;
        bool               panelRenameFocus = false;
        std::array<char, 160> dropBuffer{};
        std::vector<NotificationState> notifications;
        std::uint64_t       nextNotificationId = 1;
        ColorPickerState    colorPicker;
        std::vector<std::shared_ptr<void>> itemEffectData;
        std::shared_ptr<void> lifetime = std::make_shared<int>(0);
        void*               activeItemEffect = nullptr;
    };

    /** Internal lifecycle used by UI. */
    Context* CreateContext();
    void     DestroyContext(Context* context = nullptr);
    Context* SetCurrentContext(Context* context);
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
