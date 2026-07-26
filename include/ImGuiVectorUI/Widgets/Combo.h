// ImGuiVectorUI — animated dropdowns (ported from MYGui::ComboBox ×2).
#pragma once

#include <initializer_list>
#include "ImGuiVectorUI/Core/Types.h"

namespace IVUI
{
    // Single-select. *index selects the visible item.
    bool Combo(const char* label, int* index, std::initializer_list<const char*> items);

    // Multi-select ("N Selected"); each item toggles its own bool.
    bool ComboMulti(const char* label, std::initializer_list<ComboItem> items);
}
