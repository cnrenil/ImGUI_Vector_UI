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

    // Draws: label + primary key chip + secondary key chip + a 3-way mode selector.
    // Click a chip then press any key (keyboard or mouse) to bind; Esc clears it.
    // Returns true when a binding changed.
    bool KeybindRow(const char* label, Keybind* kb);
}
