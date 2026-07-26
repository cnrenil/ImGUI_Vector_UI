// ImGuiVectorUI — theme-driven text drawing.
// Sizes text from theme tokens via ImFont::CalcTextSizeA + ImDrawList::AddText
// (stable across ImGui versions), instead of MYGui's SetWindowFontScale /
// FontWindowScale approach (which the 1.92 font rework changes).
#pragma once

#include "imgui.h"

namespace IVUI
{
    // Font the library draws with (theme override -> context font -> ImGui default).
    ImFont* GetRegularFont();
    ImFont* GetTitleFont();

    // Everything after a '|' in a label is a disambiguation key, not shown.
    const char* VisibleEnd(const char* label);

    // pt is a theme point size; it is multiplied by the current theme Scale here.
    ImVec2 CalcTextScaled(const char* label, float pt, ImFont* font = nullptr);
    void   AddTextScaled(ImDrawList* draw, ImVec2 pos, ImU32 col, const char* label, float pt, ImFont* font = nullptr);
}
