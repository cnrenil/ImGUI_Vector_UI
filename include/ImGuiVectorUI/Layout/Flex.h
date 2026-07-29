/** @file Flex.h @brief Lightweight immediate-mode layout scopes and helpers. */
#pragma once

#include "imgui.h"

namespace IVUI
{
    /** Horizontal group. Call Next() between adjacent items. */
    class RowScope final
    {
    public:
        explicit RowScope(float spacing = -1.0f);
        ~RowScope();

        RowScope(RowScope&& other) noexcept;
        RowScope& operator=(RowScope&& other) noexcept;
        RowScope(const RowScope&) = delete;
        RowScope& operator=(const RowScope&) = delete;

        void Next();

    private:
        void Close();
        bool active_ = false;
    };

    /** Vertical group with a locally configurable item gap. */
    class ColumnScope final
    {
    public:
        explicit ColumnScope(float spacing = -1.0f);
        ~ColumnScope();

        ColumnScope(ColumnScope&& other) noexcept;
        ColumnScope& operator=(ColumnScope&& other) noexcept;
        ColumnScope(const ColumnScope&) = delete;
        ColumnScope& operator=(const ColumnScope&) = delete;

    private:
        void Close();
        bool active_ = false;
    };

    /** Borderless equal-width grid backed by an ImGui table. */
    class GridScope final
    {
    public:
        GridScope(const char* id, int columns, ImVec2 outerSize = ImVec2(0.0f, 0.0f),
                  ImGuiTableFlags flags = 0);
        ~GridScope();

        GridScope(GridScope&& other) noexcept;
        GridScope& operator=(GridScope&& other) noexcept;
        GridScope(const GridScope&) = delete;
        GridScope& operator=(const GridScope&) = delete;

        /** Advances to the next cell, automatically wrapping into a new row. */
        bool NextCell();
        explicit operator bool() const { return active_; }

    private:
        void Close();
        bool active_ = false;
    };

#ifdef IVUI_INTERNAL_API
    /** Adds empty layout space. Invalid or negative dimensions collapse to zero. */
    void Spacer(float height);
    void Spacer(ImVec2 size);

    /** Draws a themed separator, optionally with a label. */
    void Separator(const char* label = nullptr);

    /**
     * Computes a responsive column count for the current content width.
     * Set @p availableWidth to a positive value to override the current region.
     * A non-positive @p maxColumns means no explicit upper limit.
     */
    int ResponsiveColumnCount(float minColumnWidth, int maxColumns = 0,
                              float availableWidth = 0.0f, float spacing = -1.0f);
#endif

    /** RAII wrapper around ImGuiListClipper. Use while (list.Step()). */
    class VirtualListScope final
    {
    public:
        explicit VirtualListScope(int itemCount, float itemHeight = -1.0f);
        ~VirtualListScope();

        VirtualListScope(VirtualListScope&& other) noexcept;
        VirtualListScope& operator=(VirtualListScope&& other) noexcept;
        VirtualListScope(const VirtualListScope&) = delete;
        VirtualListScope& operator=(const VirtualListScope&) = delete;

        bool Step();
        int BeginIndex() const;
        int EndIndex() const;
        void Include(int itemIndex);
        void Include(int beginIndex, int endIndex);

    private:
        ImGuiListClipper* clipper_ = nullptr;
    };
}
