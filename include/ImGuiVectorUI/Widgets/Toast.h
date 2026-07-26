// ImGuiVectorUI — transient corner toast notifications.
//
// Fire-and-forget notifications that slide in from the bottom-right, stack
// upward (newest nearest the corner), auto-dismiss after a timeout, pause while
// hovered, and close on click. Drawn on the foreground draw list so they float
// above everything (and, on a transparent host, over the desktop).
//
//   IVUI::Toast(IVUI::ToastType::Success, "Saved", "Layout written");
//   ... once per frame, after building the UI:  IVUI::RenderToasts();
#pragma once

namespace IVUI
{
    enum class ToastType { Info, Success, Warning, Error };

    // Enqueue a toast. `body` may be null (title-only). `seconds` is the visible
    // duration before it auto-dismisses (hovering pauses the countdown).
    void Toast(ToastType type, const char* title, const char* body = nullptr, float seconds = 4.0f);

    // Advance + draw all live toasts. Call once per frame, near the end of the UI.
    void RenderToasts();

    // Remove every toast immediately (no fade).
    void ClearToasts();
}
