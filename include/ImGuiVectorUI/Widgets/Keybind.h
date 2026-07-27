// ImGuiVectorUI — dynamic keybind row: primary + secondary key + activation mode.
#pragma once

namespace IVUI
{
    struct Keybind
    {
        int key1 = 0; // ImGuiKey_None
        int key2 = 0;
        int mode = 0; // 0 = Toggle, 1 = Hold, 2 = Always
    };

    /** @brief Draws and edits a two-key binding with an activation mode.
     *  @param label Visible label and ImGui ID source.
     *  @param kb Binding to display and update.
     *  @return True when a key or activation mode changed.
     */
    bool KeybindRow(const char* label, Keybind* kb);
}
