// ImGuiVectorUI — id/state helpers used by widgets.
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/AnimState.h"

namespace IVUI
{
    // ImGui-ID-keyed animation slot for the current widget (respects the ID stack).
    AnimSlot& Slot(const char* label);
    AnimSlot& SlotID(ImGuiID id);

    // Clamped real frame time from the context (framerate-independent animation).
    float DeltaTime();
}
