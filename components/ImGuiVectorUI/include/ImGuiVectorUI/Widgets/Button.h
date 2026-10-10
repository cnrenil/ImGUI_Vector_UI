/** @file Button.h @brief Themed animated button widget. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Element.h"

namespace IVUI
{
    struct ButtonOptions
    {
        ImVec2 size{};
        bool prominent = false;
        ElementOptions element{};
    };

    /** @brief Draws a button.
     *  Theme values are used unless ButtonOptions::element overrides them.
     *  @return True on the activation frame.
     */
    bool Button(const char* label, const ButtonOptions& options = {});
}
