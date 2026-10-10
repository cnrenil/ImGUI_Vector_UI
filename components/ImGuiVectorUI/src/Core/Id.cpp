#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Context.h"

#include <stdexcept>

namespace IVUI
{
    AnimSlot& SlotID(ImGuiID id)
    {
        Context* c = GetContext();
        if (c == nullptr)
            throw std::logic_error("IVUI widget state requires an active UI frame.");
        return c->anim.Get(id, c->frameCount);
    }

    AnimSlot& Slot(const char* label)
    {
        return SlotID(ImGui::GetID(label));
    }

    float DeltaTime()
    {
        Context* c = GetContext();
        return c ? c->deltaTime * c->motion.speed : ImGui::GetIO().DeltaTime;
    }
}
