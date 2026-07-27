/** @file Toast.h
 *  @brief Foreground toast notifications.
 *
 *  Queue notifications with Toast() and call RenderToasts() once per frame.
 */
#pragma once

namespace IVUI
{
    enum class ToastType { Info, Success, Warning, Error };

    /** @brief Enqueues a toast; a null @p body creates a title-only notification.
     *  @param type Visual severity and icon.
     *  @param title Primary text.
     *  @param body Optional secondary text.
     *  @param seconds Visible duration before auto-dismissal.
     */
    void Toast(ToastType type, const char* title, const char* body = nullptr, float seconds = 4.0f);

    /** @brief Advances and renders queued toasts once per frame. */
    void RenderToasts();

    /** @brief Removes all active toasts immediately. */
    void ClearToasts();
}
