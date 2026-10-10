#pragma once

#include "ImGuiVectorUI/Drawing/Effects.h"

#include "imgui.h"

#include <cstdint>
#include <vector>

namespace IVUI
{
    struct ItemEffectDrawCommand
    {
        ImTextureID textureId{};
        ImVec4 clipRect{};
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
    };

    struct ItemEffectDrawData
    {
        std::vector<ImDrawVert> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<ItemEffectDrawCommand> commands;
        ImVec2 boundsMin{};
        ImVec2 boundsMax{};

        bool Empty() const noexcept
        {
            return vertices.empty() || indices.empty() || commands.empty() ||
                   boundsMax.x <= boundsMin.x || boundsMax.y <= boundsMin.y;
        }
    };
}
