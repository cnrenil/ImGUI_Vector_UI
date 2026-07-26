#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Context.h"

namespace IVUI
{
    AnimSlot& SlotID(ImGuiID id)
    {
        Context* c = GetContext();
        if (c == nullptr)
        {
            static AnimSlot dummy;
            return dummy;
        }
        return c->anim.Get(id, c->frameCount);
    }

    AnimSlot& Slot(const char* label)
    {
        return SlotID(ImGui::GetID(label));
    }

    float DeltaTime()
    {
        Context* c = GetContext();
        return c ? c->deltaTime : ImGui::GetIO().DeltaTime;
    }
}
