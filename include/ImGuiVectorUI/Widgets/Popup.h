// ImGuiVectorUI — themed confirmation popup.
#pragma once

namespace IVUI
{
    // Open the confirm popup with the given id (call on the triggering click).
    void OpenConfirm(const char* id);

    // Render the confirm popup each frame. Returns true on the frame the user
    // confirms. `id` must match OpenConfirm.
    bool ConfirmPopup(const char* id, const char* message, const char* yes = "Confirm", const char* no = "Cancel");
}
