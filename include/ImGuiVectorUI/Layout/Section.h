// ImGuiVectorUI — collapsible labeled group (ported from MYGui::FunOptions).
#pragma once

#include <functional>

namespace IVUI
{
    // Header animates its accent fill; body reveals with an eased height.
    // Left-click header toggles *open; returns true on the frame it is clicked.
    bool Section(const char* label, bool* open, const std::function<void()>& body);
}
