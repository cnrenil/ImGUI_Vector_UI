#include "ImGuiVectorUI/Layout/Flex.h"

#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace IVUI
{
    namespace
    {
        float SafeScale(const Theme& theme)
        {
            return std::isfinite(theme.Scale) && theme.Scale > 0.0f ? theme.Scale : 1.0f;
        }

        float ResolveSpacing(float spacing, float fallback)
        {
            if (std::isfinite(spacing) && spacing >= 0.0f)
                return spacing;
            return std::isfinite(fallback) && fallback >= 0.0f ? fallback : 0.0f;
        }

        ImVec2 NonNegativeSize(ImVec2 size)
        {
            if (!std::isfinite(size.x) || size.x < 0.0f)
                size.x = 0.0f;
            if (!std::isfinite(size.y) || size.y < 0.0f)
                size.y = 0.0f;
            return size;
        }
    }

    RowScope::RowScope(float spacing)
    {
        const Theme& theme = GetTheme();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(ResolveSpacing(spacing, theme.ItemSpacing * SafeScale(theme)),
                                   style.ItemSpacing.y));
        ImGui::BeginGroup();
        active_ = true;
    }

    RowScope::~RowScope()
    {
        Close();
    }

    RowScope::RowScope(RowScope&& other) noexcept : active_(std::exchange(other.active_, false))
    {
    }

    RowScope& RowScope::operator=(RowScope&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    void RowScope::Next()
    {
        if (active_)
            ImGui::SameLine();
    }

    void RowScope::Close()
    {
        if (!active_)
            return;
        ImGui::EndGroup();
        ImGui::PopStyleVar();
        active_ = false;
    }

    ColumnScope::ColumnScope(float spacing)
    {
        const Theme& theme = GetTheme();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(style.ItemSpacing.x,
                                   ResolveSpacing(spacing, theme.ItemSpacing * SafeScale(theme))));
        ImGui::BeginGroup();
        active_ = true;
    }

    ColumnScope::~ColumnScope()
    {
        Close();
    }

    ColumnScope::ColumnScope(ColumnScope&& other) noexcept : active_(std::exchange(other.active_, false))
    {
    }

    ColumnScope& ColumnScope::operator=(ColumnScope&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    void ColumnScope::Close()
    {
        if (!active_)
            return;
        ImGui::EndGroup();
        ImGui::PopStyleVar();
        active_ = false;
    }

    GridScope::GridScope(const char* id, int columns, ImVec2 outerSize, ImGuiTableFlags flags)
    {
        if (columns <= 0)
            return;
        columns = std::min(columns, IMGUI_TABLE_MAX_COLUMNS);

        const Theme& theme = GetTheme();
        const float scale = SafeScale(theme);
        outerSize = NonNegativeSize(outerSize);
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, theme.BorderMuted);
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, theme.Border);
        const float spacing = ResolveSpacing(theme.ItemSpacing * scale, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                            ImVec2(spacing * 0.5f, spacing * 0.5f));
        if ((flags & ImGuiTableFlags_SizingMask_) == 0)
            flags |= ImGuiTableFlags_SizingStretchSame;
        flags |= ImGuiTableFlags_NoSavedSettings;
        active_ = ImGui::BeginTable(id && id[0] != '\0' ? id : "##ivui_grid", columns, flags, outerSize);
        if (!active_)
        {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }

    GridScope::~GridScope()
    {
        Close();
    }

    GridScope::GridScope(GridScope&& other) noexcept : active_(std::exchange(other.active_, false))
    {
    }

    GridScope& GridScope::operator=(GridScope&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    bool GridScope::NextCell()
    {
        return active_ && ImGui::TableNextColumn();
    }

    void GridScope::Close()
    {
        if (!active_)
            return;
        ImGui::EndTable();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        active_ = false;
    }

    void Spacer(float height)
    {
        Spacer(ImVec2(0.0f, height));
    }

    void Spacer(ImVec2 size)
    {
        ImGui::Dummy(NonNegativeSize(size));
    }

    void Separator(const char* label)
    {
        const Theme& theme = GetTheme();
        ImGui::PushStyleColor(ImGuiCol_Separator, theme.BorderMuted);
        ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, theme.AccentHover);
        ImGui::PushStyleColor(ImGuiCol_SeparatorActive, theme.AccentActive);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.TextMuted);
        if (label && label[0] != '\0')
            ImGui::SeparatorText(label);
        else
            ImGui::Separator();
        ImGui::PopStyleColor(4);
    }

    int ResponsiveColumnCount(float minColumnWidth, int maxColumns,
                              float availableWidth, float spacing)
    {
        const Theme& theme = GetTheme();
        const float scale = SafeScale(theme);
        if (!std::isfinite(availableWidth) || availableWidth <= 0.0f)
            availableWidth = ImGui::GetContentRegionAvail().x;
        if (!std::isfinite(minColumnWidth) || minColumnWidth <= 0.0f)
            minColumnWidth = 1.0f;
        spacing = ResolveSpacing(spacing, theme.ItemSpacing * scale);

        const double width = static_cast<double>(std::max(availableWidth, 0.0f));
        const double columnWidth = static_cast<double>(minColumnWidth);
        const double gap = static_cast<double>(spacing);
        const double rawColumns = (width + gap) / (columnWidth + gap);
        int columns = static_cast<int>(std::clamp(rawColumns, 1.0,
                                                   static_cast<double>(IMGUI_TABLE_MAX_COLUMNS)));
        if (maxColumns > 0)
            columns = std::min(columns, maxColumns);
        return columns;
    }

    VirtualListScope::VirtualListScope(int itemCount, float itemHeight)
        : clipper_(new ImGuiListClipper())
    {
        itemCount = std::max(itemCount, 0);
        if (!std::isfinite(itemHeight) || itemHeight <= 0.0f)
            itemHeight = -1.0f;
        clipper_->Begin(itemCount, itemHeight);
    }

    VirtualListScope::~VirtualListScope()
    {
        delete clipper_;
    }

    VirtualListScope::VirtualListScope(VirtualListScope&& other) noexcept
        : clipper_(std::exchange(other.clipper_, nullptr))
    {
    }

    VirtualListScope& VirtualListScope::operator=(VirtualListScope&& other) noexcept
    {
        if (this != &other)
        {
            delete clipper_;
            clipper_ = std::exchange(other.clipper_, nullptr);
        }
        return *this;
    }

    bool VirtualListScope::Step()
    {
        return clipper_ && clipper_->Step();
    }

    int VirtualListScope::BeginIndex() const
    {
        return clipper_ ? clipper_->DisplayStart : 0;
    }

    int VirtualListScope::EndIndex() const
    {
        return clipper_ ? clipper_->DisplayEnd : 0;
    }

    void VirtualListScope::Include(int itemIndex)
    {
        if (clipper_ && itemIndex >= 0 && itemIndex < clipper_->ItemsCount)
            clipper_->IncludeItemByIndex(itemIndex);
    }

    void VirtualListScope::Include(int beginIndex, int endIndex)
    {
        if (!clipper_)
            return;
        beginIndex = std::max(beginIndex, 0);
        endIndex = std::min(std::max(endIndex, beginIndex), clipper_->ItemsCount);
        if (beginIndex < endIndex)
            clipper_->IncludeItemsByIndex(beginIndex, endIndex);
    }
}
