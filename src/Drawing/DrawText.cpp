#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h" // ImFloor

#include <string.h>

namespace IVUI
{
    ImFont* GetRegularFont()
    {
        const Theme& t = GetTheme();
        if (t.FontRegular)
            return t.FontRegular;
        if (Context* c = GetContext())
            if (c->fontRegular)
                return c->fontRegular;
        return ImGui::GetFont();
    }

    ImFont* GetTitleFont()
    {
        const Theme& t = GetTheme();
        if (t.FontTitle)
            return t.FontTitle;
        if (Context* c = GetContext())
            if (c->fontTitle)
                return c->fontTitle;
        return GetRegularFont();
    }

    const char* VisibleEnd(const char* label)
    {
        // Hide everything from the first "##"/"###" (ImGui id marker) or '|'.
        for (const char* p = label; *p; ++p)
        {
            if ((p[0] == '#' && p[1] == '#') || p[0] == '|')
                return p;
        }
        return label + strlen(label);
    }

    ImVec2 CalcTextScaled(const char* label, float pt, ImFont* font)
    {
        if (font == nullptr)
            font = GetRegularFont();
        const float size = pt * GetTheme().Scale;
        return font->CalcTextSizeA(size, FLT_MAX, 0.0f, label, VisibleEnd(label));
    }

    void AddTextScaled(ImDrawList* draw, ImVec2 pos, ImU32 col, const char* label, float pt, ImFont* font)
    {
        if (font == nullptr)
            font = GetRegularFont();
        const float size = pt * GetTheme().Scale;
        // Snap to whole pixels so glyphs land on the pixel grid (crisper text).
        pos.x = ImFloor(pos.x);
        pos.y = ImFloor(pos.y);
        draw->AddText(font, size, pos, col, label, VisibleEnd(label));
    }
}
