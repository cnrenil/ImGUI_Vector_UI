/** @file Id.h @brief ImGui-ID-based widget state helpers. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/AnimState.h"

namespace IVUI
{
    /** @brief Returns an animation slot using ImGui's current ID stack. */
    AnimSlot& Slot(const char* label);
    AnimSlot& SlotID(ImGuiID id);

    /** @brief Returns the context's clamped frame time. */
    float DeltaTime();
}
