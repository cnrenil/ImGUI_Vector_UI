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

    /** @brief Draws an active-feature HUD using the requested or active-theme layout.
     *  @param anchorTopRight Screen-space top-right anchor.
     *  @param labels Active feature labels.
     *  @param count Number of entries in @p labels.
     *  @param options Optional visual layout override.
     */
    void StatusList(ImVec2 anchorTopRight, const char* const* labels, int count,
                    const StatusListOptions& options = StatusListOptions());

    /** @brief Draws legacy top-right accent chips.
     *  @param anchorTopRight Screen-space top-right anchor.
     *  @param labels Active feature labels.
     *  @param count Number of entries in @p labels.
     */
    void StatusChips(ImVec2 anchorTopRight, const char* const* labels, int count);
    /** @brief Draws legacy accent chips from an initializer list.
     *  @param anchorTopRight Screen-space top-right anchor.
     *  @param activeLabels Active feature labels.
     */
    void StatusChips(ImVec2 anchorTopRight, std::initializer_list<const char*> activeLabels);
}
