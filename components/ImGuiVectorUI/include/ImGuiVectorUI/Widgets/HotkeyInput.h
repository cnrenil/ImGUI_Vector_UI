/** @file HotkeyInput.h @brief Keyboard and mouse key-capture widget. */
#pragma once

namespace IVUI
{
    /** @brief Captures the next input into @p key.
     *  @return True on the frame a new key is bound.
     */
    bool HotkeyInput(const char* label, int* key);
}
