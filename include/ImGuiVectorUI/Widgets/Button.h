// ImGuiVectorUI — themed animated button.
#pragma once

#include "imgui.h"

namespace IVUI
{
    // prominent -> filled accent; otherwise a subtle surface button.
    bool Button(const char* label, ImVec2 size = ImVec2(0, 0), bool prominent = false);
}
