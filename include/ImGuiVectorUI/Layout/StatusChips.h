// ImGuiVectorUI — always-on "active features" HUD chip list.
#pragma once

#include "imgui.h"
#include <initializer_list>

namespace IVUI
{
    enum class StatusListStyle { ThemeDefault, Chips, SideRail, Marquee, GlassStack };

    struct StatusListOptions
    {
        StatusListStyle style = StatusListStyle::ThemeDefault;
    };

    // Draw an active-feature HUD. ThemeDefault resolves to the active theme's
    // signature layout: VAPE side rail, Neverlose RGB frame, iOS glass stack, etc.
    void StatusList(ImVec2 anchorTopRight, const char* const* labels, int count,
                    const StatusListOptions& options = StatusListOptions());

    // Draw a top-right-anchored vertical stack of accent pill chips (one per active
    // feature label) on the foreground draw list. Retained for API compatibility.
    void StatusChips(ImVec2 anchorTopRight, const char* const* labels, int count);
    void StatusChips(ImVec2 anchorTopRight, std::initializer_list<const char*> activeLabels);
}
