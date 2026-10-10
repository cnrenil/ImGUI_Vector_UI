#pragma once

#include "imgui.h"
#include <functional>
#include <span>

namespace IVUI
{
    class UI;

    enum class ContentLayout { Tiled, Columns, Windows };

    struct ContentBlock
    {
        const char* id = nullptr; ///< Stable, nonempty and unique within the layout.
        const char* title = nullptr;
        std::function<void(UI&)> draw;
        float weight = 1.0f; ///< Relative column width; ignored by tiled layouts.
        bool* open = nullptr; ///< Optional host-owned visibility/close state.
        ImVec2 initialPosition{-1, -1}; ///< Windows only; physical screen coordinates.
        ImVec2 initialSize{}; ///< Windows only; zero uses the layout default.
    };

    struct ContentLayoutOptions
    {
        ContentLayout mode = ContentLayout::Tiled;
        const char* title = nullptr;
        int columns = 0; ///< Zero chooses by minimumColumnWidth; narrow views reflow.
        float minimumColumnWidth = 260.0f; ///< Unscaled design units.
        float tileHeight = 240.0f;
        ImVec2 initialSize{960, 640}; ///< Unscaled window size for single-window modes.
        bool embedded = false; ///< Tiled/Columns: use the current window instead.
        bool* open = nullptr; ///< Single-window visibility/close state.
        ImGuiWindowFlags windowFlags = 0;
    };

    /** Draw the same content as tiles, scrolling columns or separate ImGui windows.
     *  No native windows/devices are created. False rejects invalid/duplicate IDs
     *  before drawing. Call once per frame; callbacks and strings are not retained.
     */
    bool RenderContentLayout(UI& ui, const char* id, std::span<const ContentBlock> blocks,
                             const ContentLayoutOptions& options = {});
}
