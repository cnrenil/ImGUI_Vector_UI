// ImGuiVectorUI — shared small types.
#pragma once

#include "imgui.h"

namespace IVUI
{
    // One entry in a multi-select combo: a visible label + the bool it toggles.
    struct ComboItem
    {
        const char* label;
        bool*       open;
    };
}
