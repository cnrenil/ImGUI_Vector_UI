#include "ImGuiVectorUI/Widgets/Combo.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui_internal.h"

#include <cstdio>
#include <functional>

namespace IVUI
{
    // Draw the header (label + closed box + current text + chevron) and place the
    // header hit button. Returns the header rect end (top of the dropdown) and the
    // usable width; toggles `a.hidden` (reused as "is open") on click.
    static ImVec2 ComboHeader(const char* label, const char* currentText, AnimSlot& a, float& width, float& rowH)
    {
        const Theme& th = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;
        width = ImMax(avail.x - 12.0f * S, 60.0f * S);
        rowH = th.ControlHeight * S;

        const ImVec2 labelSize = CalcTextScaled(label, th.FontSizeSmall);
        AddTextScaled(draw, cursor, th.TextMuted, label, th.FontSizeSmall);
        ImGui::Dummy(ImVec2(width, labelSize.y + 4.0f * S));

        const ImVec2 bgPos = ImVec2(cursor.x, cursor.y + labelSize.y + 4.0f * S);
        const ImVec2 bgEnd = ImVec2(bgPos.x + width, bgPos.y + rowH);
        draw->AddRectFilled(bgPos, bgEnd, WithAlphaMul(th.Surface2, th.SurfaceAlpha), th.RoundingControl * S, ImDrawFlags_RoundCornersTop);
        draw->AddLine(ImVec2(bgPos.x, bgEnd.y), ImVec2(bgEnd.x, bgEnd.y), th.Accent, ImMax(2.0f * S, 1.0f));
        Sheen(draw, bgPos, bgEnd, th.RoundingControl * S, th.Sheen);

        const ImVec2 curSize = CalcTextScaled(currentText, th.FontSizeLabel);
        AddTextScaled(draw, ImVec2(bgPos.x + 8.0f * S, bgPos.y + (rowH - curSize.y) * 0.5f), th.Text, currentText, th.FontSizeLabel);
        IconChevron(draw, ImVec2(bgEnd.x - 16.0f * S, bgPos.y + rowH * 0.5f), 14.0f * S, th.TextMuted,
                    ImMax(1.6f * S, 1.0f), a.hidden ? 1 : 0);

        ImGui::SetCursorScreenPos(bgPos);
        if (ImGui::InvisibleButton(label, ImVec2(width, rowH)))
            a.hidden = !a.hidden; // "is open"

        // Dropdown starts at the header's bottom-LEFT (not bottom-right).
        return ImVec2(bgPos.x, bgEnd.y);
    }

    // Draw one dropdown row (bg + check icon + text). `on` drives the highlight ease.
    // The caller places an InvisibleButton at the row before calling this.
    static void ComboRow(AnimSlot& a, int index, const char* text, float width, float rowH, bool on)
    {
        const Theme& th = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float S = th.Scale;
        const ImVec2 ip = ImGui::GetItemRectMin(); // the row InvisibleButton just submitted

        ItemAnim& it = a.Item(index);
        it.direction = on ? 1.0f : -1.0f;
        it.t = ImClamp(it.t + DeltaTime() * 8.0f * it.direction, 0.0f, 1.0f);

        WidgetGlow(draw, ip, ImVec2(ip.x + width, ip.y + rowH), th.Accent, 0.0f, it.t);
        const ImU32 bg = EaseValueColor(ImColor(th.Surface1), ImColor(th.Accent), it.t, EASE_IN_SINE);
        draw->AddRectFilled(ip, ImVec2(ip.x + width, ip.y + rowH), bg);

        const float iconSz = 18.0f * S;
        const ImU32 iconCol = EaseValueColor(ImColor(0, 0, 0, 0), ImColor(th.OnAccent), it.t, EASE_IN_SINE);
        IconCheck(draw, ImVec2(ip.x + 8.0f * S + iconSz * 0.5f, ip.y + rowH * 0.5f), iconSz, iconCol, ImMax(1.6f * S, 1.0f));

        const ImVec2 txtSize = CalcTextScaled(text, th.FontSizeLabel);
        const float txtX = ip.x + 8.0f * S + EaseValue(0.0f, iconSz + 8.0f * S, it.t, EASE_IN_SINE);
        const ImU32 txtCol = EaseValueColor(ImColor(th.Text), ImColor(th.OnAccent), it.t, EASE_IN_SINE);
        AddTextScaled(draw, ImVec2(txtX, ip.y + (rowH - txtSize.y) * 0.5f), txtCol, text, th.FontSizeLabel);
    }

    // Render the dropdown rows inside a clipped child of known (animated) height.
    // Rows are a fixed height, so the target height is exactly count*rowH — no
    // fragile content measurement needed.
    static void ComboReveal(const char* label, AnimSlot& a, ImVec2 dropTop, float width, float rowH,
                            int count, const std::function<void(int)>& row)
    {
        a.sizeY = count * rowH;
        const float visibleH = EaseValue(0.0f, a.sizeY, a.t, EASE_OUT_CUBIC);
        ImGui::SetCursorScreenPos(dropTop);
        ImGui::PushID(label);
        ImGui::BeginChild("ivui_drop", ImVec2(width, ImMax(visibleH, 1.0f)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            row(i);
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopID();
    }

    bool Combo(const char* label, int* index, std::initializer_list<const char*> items)
    {
        AnimSlot& a = Slot(label);
        const int count = (int)items.size();
        if (count <= 0)
            return false;
        *index = ImClamp(*index, 0, count - 1);

        float width, rowH;
        const ImVec2 dropTop = ComboHeader(label, *(items.begin() + *index), a, width, rowH);

        const bool open = a.hidden;
        a.direction = open ? 1.0f : -1.0f;
        a.t = ImClamp(a.t + DeltaTime() * 8.0f * a.direction, 0.0f, 1.0f);

        bool changed = false;
        if (a.t > 0.001f)
        {
            ComboReveal(label, a, dropTop, width, rowH, count, [&](int i)
            {
                if (ImGui::InvisibleButton("ivui_row", ImVec2(width, rowH))) { *index = i; changed = true; }
                ComboRow(a, i, *(items.begin() + i), width, rowH, (*index == i));
            });
        }
        return changed;
    }

    bool ComboMulti(const char* label, std::initializer_list<ComboItem> items)
    {
        AnimSlot& a = Slot(label);
        const int count = (int)items.size();
        if (count <= 0)
            return false;

        int selected = 0;
        for (const ComboItem& it : items)
            if (it.open && *it.open)
                ++selected;
        a.opened = selected;

        char header[64];
        snprintf(header, sizeof(header), "%d Selected", selected);

        float width, rowH;
        const ImVec2 dropTop = ComboHeader(label, header, a, width, rowH);

        const bool open = a.hidden;
        a.direction = open ? 1.0f : -1.0f;
        a.t = ImClamp(a.t + DeltaTime() * 8.0f * a.direction, 0.0f, 1.0f);

        bool changed = false;
        if (a.t > 0.001f)
        {
            ComboReveal(label, a, dropTop, width, rowH, count, [&](int i)
            {
                const ComboItem& item = *(items.begin() + i);
                if (ImGui::InvisibleButton("ivui_row", ImVec2(width, rowH)))
                    if (item.open) { *item.open = !*item.open; changed = true; }
                ComboRow(a, i, item.label, width, rowH, (item.open && *item.open));
            });
        }
        return changed;
    }
}
