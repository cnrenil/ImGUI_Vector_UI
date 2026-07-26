// ImGuiVectorUI — themed wrapper around ImGui tables.
#pragma once

#include "imgui.h"

namespace IVUI
{
    // Themed table. Defaults to RowBg + inner borders + scroll + sortable + resizable.
    // Use ImGui::TableSetupColumn / TableHeadersRow / TableNextRow / TableNextColumn
    // between Begin/End as usual. `outerSize` sizes the scroll region.
    bool BeginTable(const char* id, int columns, ImVec2 outerSize = ImVec2(0, 0), ImGuiTableFlags extraFlags = 0);
    void EndTable();
}
