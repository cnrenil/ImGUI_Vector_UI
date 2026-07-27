/// ImGuiVectorUI — vertical sidebar tab with icon + animated selection.
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Drawing/Icons.h"

namespace IVUI
{
    /// One full-width sidebar row. Animates its background + a left accent bar when
    /// active. Sets *active to `index` on click; returns true on that frame.
    bool Tab(const char* label, IconType icon, int index, int* active);
}
