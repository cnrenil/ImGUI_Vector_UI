/** @file Popup.h @brief Themed confirmation popup helpers. */
#pragma once

namespace IVUI
{
    /** @brief Opens the confirmation popup identified by @p id. */
    void OpenConfirm(const char* id);

    /** @brief Renders a confirmation popup opened with the matching @p id.
     *  @return True on the frame the user confirms.
     */
    bool ConfirmPopup(const char* id, const char* message, const char* yes = "Confirm", const char* no = "Cancel");
}
