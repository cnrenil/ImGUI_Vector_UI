// ImGuiVectorUI — hotkey capture field. Click, then press a key; *key receives
// the ImGuiKey. Returns true on the frame a new key is bound.
#pragma once

namespace IVUI
{
    bool HotkeyInput(const char* label, int* key);
}
