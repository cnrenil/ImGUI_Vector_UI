/** @file ColorPicker.h @brief Themed color picker widget. */
#pragma once

#include "imgui.h"

namespace IVUI
{
    /** @brief Draws a labeled color swatch and popup editor.
     *  @return True when @p col changed.
     */
    bool ColorPicker(const char* label, ImVec4* col);
}
