/** @file Notification.h @brief Handle-based foreground notifications. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Drawing/Effects.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace IVUI
{
    struct NotificationHandle
    {
        std::uint64_t value = 0;

        explicit operator bool() const noexcept { return value != 0; }
        friend bool operator==(NotificationHandle left, NotificationHandle right) noexcept
        {
            return left.value == right.value;
        }
        friend bool operator!=(NotificationHandle left, NotificationHandle right) noexcept
        {
            return !(left == right);
        }
    };

    enum class NotificationType { Info, Success, Warning, Error };
    enum class NotificationStyle { Card, Glass, Compact, Banner, DynamicIsland };
    enum class NotificationPlacement
    {
        TopLeft,
        TopCenter,
        TopRight,
        BottomLeft,
        BottomCenter,
        BottomRight,
        Custom
    };

    struct NotificationTheme
    {
        NotificationStyle style = NotificationStyle::Card;
        NotificationPlacement placement = NotificationPlacement::BottomRight;
        ImVec2 viewportPadding{18.0f, 18.0f};
        ImVec2 offset{};
        float width = 300.0f;       ///< Zero lets content determine the width.
        float maxWidth = 420.0f;
        float spacing = 10.0f;
        float duration = 4.0f;      ///< Zero keeps notifications until closed.
        bool pauseOnHover = true;
        bool dismissOnClick = true;
        bool showProgress = true;
        bool clampToViewport = true;
        bool deduplicate = true;
        std::size_t queueLimit = 8; ///< Zero means unlimited.
    };

    struct NotificationAction
    {
        const char* label = nullptr;
        void (*callback)(NotificationHandle handle, void* userData) = nullptr;
        void* userData = nullptr;
        bool dismiss = true;
    };

    /** Unset fields inherit NotificationTheme and ElementRole::Notification. */
    struct NotificationOptions
    {
        std::optional<NotificationType> type;
        std::optional<NotificationStyle> style;
        std::optional<NotificationPlacement> placement;
        std::optional<ImVec2> position; ///< Required only for Custom placement.
        std::optional<ImVec2> offset;
        std::optional<ImVec2> viewportPadding;
        std::optional<float> width;
        std::optional<float> maxWidth;
        std::optional<float> spacing;
        std::optional<float> duration;
        std::optional<bool> pauseOnHover;
        std::optional<bool> dismissOnClick;
        std::optional<bool> showProgress;
        std::optional<bool> clampToViewport;
        std::optional<float> progress; ///< Explicit determinate progress in [0, 1].
        std::optional<std::string> dedupeKey;
        std::optional<ItemEffectRequest> effects;
        std::optional<ItemMotionStyle> motion;
        std::optional<NotificationAction> action;
    };

    /** Fields supplied here replace only the corresponding fields on a live handle. */
    struct NotificationUpdate
    {
        std::optional<std::string> title;
        std::optional<std::string> body;
        NotificationOptions options;
    };

#ifdef IVUI_INTERNAL_API
    NotificationHandle Notify(const char* title, const char* body = nullptr,
                              const NotificationOptions& options = {});
    bool UpdateNotification(NotificationHandle handle, const NotificationUpdate& update);
    bool CloseNotification(NotificationHandle handle);
    bool SetNotificationProgress(NotificationHandle handle, float progress);
    void ClearNotifications();
    void RenderNotifications();
#endif
}
