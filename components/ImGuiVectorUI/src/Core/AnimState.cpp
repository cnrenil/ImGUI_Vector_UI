#include "ImGuiVectorUI/Core/AnimState.h"

namespace IVUI
{
    ItemAnim& AnimSlot::Item(int index)
    {
        ItemAnim* p = (ItemAnim*)items.GetVoidPtr((ImGuiID)index);
        if (p == nullptr)
        {
            p = IM_NEW(ItemAnim)();
            items.SetVoidPtr((ImGuiID)index, p);
        }
        return *p;
    }

    static void FreeSlot(AnimSlot* slot)
    {
        for (int i = 0; i < slot->items.Data.Size; ++i)
            IM_DELETE((ItemAnim*)slot->items.Data[i].val_p);
        IM_DELETE(slot);
    }

    AnimSlot& AnimRegistry::Get(ImGuiID id, unsigned frame)
    {
        AnimSlot* slot = (AnimSlot*)store.GetVoidPtr(id);
        if (slot == nullptr)
        {
            slot = IM_NEW(AnimSlot)();
            store.SetVoidPtr(id, slot);
        }
        slot->lastFrame = frame;
        return *slot;
    }

    void AnimRegistry::GarbageCollect(unsigned frame, unsigned maxAge)
    {
        // Do not mutate ImGuiStorage while enumerating it.
        ImVector<ImGuiID> dead;
        for (int i = 0; i < store.Data.Size; ++i)
        {
            AnimSlot* slot = (AnimSlot*)store.Data[i].val_p;
            if (slot && (frame - slot->lastFrame) > maxAge)
                dead.push_back(store.Data[i].key);
        }
        for (int i = 0; i < dead.Size; ++i)
        {
            AnimSlot* slot = (AnimSlot*)store.GetVoidPtr(dead[i]);
            if (slot)
                FreeSlot(slot);
            store.SetVoidPtr(dead[i], nullptr);
        }
    }

    void AnimRegistry::Clear()
    {
        for (int i = 0; i < store.Data.Size; ++i)
            if (AnimSlot* slot = (AnimSlot*)store.Data[i].val_p)
                FreeSlot(slot);
        store.Clear();
    }
}
