/** @file Button.h @brief Themed animated button widget. */
#pragma once

#include "imgui.h"

namespace IVUI
{
    /** @brief Draws a button.
     *  @param prominent Uses the accent-filled visual treatment when true.
     *  @return True on the activation frame.
     */
    bool Button(const char* label, ImVec2 size = ImVec2(0, 0), bool prominent = false);
}
