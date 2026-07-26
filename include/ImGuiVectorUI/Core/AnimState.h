// ImGuiVectorUI — per-widget animation state.
// Replaces MYGui's global name-keyed std::maps with an ImGuiID-keyed registry
// owned by the context: no per-frame string allocations, correct multi-instance
// behavior via ImGui's ID stack, and generation-based garbage collection.
#pragma once

#include "imgui.h"

namespace IVUI
{
    // Per-item animation (combo rows).
    struct ItemAnim
    {
        float t = 0.0f;
        float direction = 1.0f;
    };

    // The animated-state bundle for one widget instance (MYGui's "Winclas" + the
    // slider/section/combo extras, unified).
    struct AnimSlot
    {
        float t = 0.0f;            // primary 0..1 progress
        float direction = 1.0f;
        float sizeY = 0.0f;        // measured content height (auto-size containers)

        float t2 = 0.0f;           // secondary progress (section header color)
        float direction2 = 1.0f;

        float currentValue = 0.0f; // slider internal value (segments/labeled)
        float currentPercent = 0.0f;
        float lastTarget = -1.0f;

        bool  hidden = false;      // collapsed / closed
        bool  capturing = false;   // hotkey capture in progress
        bool  initialized = false; // Motion helpers: seeded to target on first use
        int   opened = 0;          // multi-combo selected count
        ImVec4 colorVal{};         // Motion: animated color state

        unsigned  lastFrame = 0;   // for GC
        ImGuiStorage items;        // int index -> ItemAnim*

        ItemAnim& Item(int index);
    };

    // ImGuiID -> AnimSlot* registry.
    struct AnimRegistry
    {
        ImGuiStorage store;

        AnimSlot& Get(ImGuiID id, unsigned frame);
        void      GarbageCollect(unsigned frame, unsigned maxAge);
        void      Clear();
    };
}
