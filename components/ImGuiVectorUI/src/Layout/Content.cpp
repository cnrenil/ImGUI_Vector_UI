#include "ImGuiVectorUI/Layout/Content.h"
#include "ImGuiVectorUI/UI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace IVUI
{
    namespace
    {
        float Positive(float value, float fallback)
        {
            return std::isfinite(value) && value > 0 ? value : fallback;
        }

        std::string WindowName(const char* title, const char* layout, const char* block = nullptr)
        {
            // Length-prefix segments so user IDs containing separators cannot alias.
            std::string name = title ? title : layout;
            name += "###ivui-content:" + std::to_string(std::strlen(layout)) + ":" + layout;
            if (block) name += ":" + std::to_string(std::strlen(block)) + ":" + block;
            return name;
        }

        void DrawBlock(UI& ui, const ContentBlock& block)
        {
            if (block.title && *block.title)
                ImGui::SeparatorText(block.title);
            if (block.draw) block.draw(ui);
        }
    }

    bool RenderContentLayout(UI& ui, const char* id, std::span<const ContentBlock> blocks,
                             const ContentLayoutOptions& options)
    {
        if (!ui.IsFrameActive() || !id || !*id) return false;
        if (options.mode != ContentLayout::Tiled && options.mode != ContentLayout::Columns &&
            options.mode != ContentLayout::Windows) return false;
        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            if (!blocks[i].id || !*blocks[i].id) return false;
            for (std::size_t j = 0; j < i; ++j)
                if (std::strcmp(blocks[i].id, blocks[j].id) == 0) return false;
        }
        if (options.open && !*options.open) return true;
        const float scale = Positive(ui.theme().Current().Scale, 1.0f);
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (options.mode == ContentLayout::Windows)
        {
            std::size_t index = 0;
            for (const ContentBlock& block : blocks)
            {
                if (block.open && !*block.open) continue;
                const ImVec2 size(std::min(Positive(block.initialSize.x, 360 * scale), viewport->WorkSize.x),
                                  std::min(Positive(block.initialSize.y, 320 * scale), viewport->WorkSize.y));
                const float offset = 24 * scale * static_cast<float>(index++ % 8);
                const ImVec2 position(
                    std::isfinite(block.initialPosition.x) && block.initialPosition.x >= 0
                        ? block.initialPosition.x : viewport->WorkPos.x + std::min(offset, std::max(viewport->WorkSize.x - size.x, 0.0f)),
                    std::isfinite(block.initialPosition.y) && block.initialPosition.y >= 0
                        ? block.initialPosition.y : viewport->WorkPos.y + std::min(offset, std::max(viewport->WorkSize.y - size.y, 0.0f)));
                ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
                const std::string name = WindowName(block.title ? block.title : block.id, id, block.id);
                if (ImGui::Begin(name.c_str(), block.open, options.windowFlags))
                {
                    if (block.draw) block.draw(ui);
                }
                ImGui::End();
            }
            return true;
        }

        bool visible = true;
        if (!options.embedded)
        {
            const ImVec2 size(std::min(Positive(options.initialSize.x, 960) * scale, viewport->WorkSize.x),
                              std::min(Positive(options.initialSize.y, 640) * scale, viewport->WorkSize.y));
            ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
            const std::string name = WindowName(options.title, id);
            visible = ImGui::Begin(name.c_str(), options.open, options.windowFlags);
        }
        if (visible)
        {
            std::vector<const ContentBlock*> active;
            active.reserve(blocks.size());
            for (const auto& block : blocks)
                if (!block.open || *block.open) active.push_back(&block);
            if (!active.empty())
            {
                ImGui::PushID(id);
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const float minWidth = Positive(options.minimumColumnWidth, 260) * scale;
                const int limit = static_cast<int>(std::min<std::size_t>(active.size(), 64));
                const int requested = options.columns > 0 ? std::min(options.columns, limit) : limit;
                const int columns = ui.layouts().ResponsiveColumnCount(minWidth, requested);
                const float height = options.mode == ContentLayout::Columns
                    ? std::max(available.y, 1.0f) : Positive(options.tileHeight, 240) * scale;
                if (ImGui::BeginTable("##blocks", columns, ImGuiTableFlags_SizingStretchSame |
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
                {
                    for (int column = 0; column < columns; ++column)
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch,
                            options.mode == ContentLayout::Columns
                                ? Positive(active[column]->weight, 1) : 1.0f);
                    for (const ContentBlock* block : active)
                    {
                        ImGui::TableNextColumn();
                        ImGui::PushID(block->id);
                        if (ImGui::BeginChild("##block", ImVec2(0, height),
                            ImGuiChildFlags_AlwaysUseWindowPadding))
                            DrawBlock(ui, *block);
                        ImGui::EndChild();
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::PopID();
            }
        }
        if (!options.embedded) ImGui::End();
        return true;
    }
}
