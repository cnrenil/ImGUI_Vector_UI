#pragma once

#include "imgui.h"
#include <functional>

namespace IVUI
{
    struct WorkbenchOptions
    {
        const char* title = "";
        const char* status = nullptr;
        ImVec2 size{}; ///< Zero fills available space; physical pixels.
        float sidebarWidth = 192.0f; ///< Design units scaled by Theme::Scale.
        float compactBreakpoint = 640.0f;
        bool transparentSurface = false; ///< Let a submitted glass effect provide the main surface.
    };

    /** Header, responsive navigation, scrolling body and optional status band.
     *  Navigation receives true in compact mode; use a combo or horizontal selector.
     */
    void Workbench(const char* id, const WorkbenchOptions& options,
                   const std::function<void(bool compact)>& navigation,
                   const std::function<void()>& body);
}
