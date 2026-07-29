#include "ImGuiVectorUI/Widgets/HotkeyInput.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"

#include "imgui_internal.h"

namespace IVUI
{
    bool HotkeyInput(const char* label, int* key)
    {
        const Theme& th = GetTheme();
        AnimSlot& a = Slot(label);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;
        const float h = th.ControlHeight * S * 0.8f;
        const float boxW = 120.0f * S;

        const ImVec2 ls = CalcTextScaled(label, th.FontSizeLabel);
        AddTextScaled(draw, ImVec2(cursor.x, cursor.y + (h - ls.y) * 0.5f), th.Text, label, th.FontSizeLabel);

        const float rightInset = 12.0f * S;
        const ImVec2 boxPos = ImVec2(cursor.x + ImMax(avail.x - rightInset - boxW, 0.0f), cursor.y);
        const ImVec2 boxEnd = boxPos + ImVec2(boxW, h);

        ImGui::SetCursorScreenPos(boxPos);
        if (ImGui::InvisibleButton(label, ImVec2(boxW, h), ImGuiButtonFlags_EnableNav))
            a.capturing = true;

        bool changed = false;
        if (a.capturing)
        {
            // Prevent host shortcuts from consuming the key being captured.
            if (Context* c = GetContext())
                c->wantCaptureHotkey = true;

            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
            {
                if (k >= ImGuiKey_MouseLeft && k <= ImGuiKey_MouseWheelY)
                    continue; // don't bind the mouse that opened capture
                if (ImGui::IsKeyPressed((ImGuiKey)k, false))
                {
                    a.capturing = false;
                    if (k != ImGuiKey_Escape) { *key = k; changed = true; }
                    break;
                }
            }
        }

        const ImU32 bg = a.capturing ? th.Accent : th.Surface2;
        draw->AddRectFilled(boxPos, boxEnd, bg, th.RoundingControl * S);
        draw->AddRect(boxPos, boxEnd, th.BorderMuted, th.RoundingControl * S, 0, th.BorderThickness * S);

        const char* name = a.capturing ? "[Press key]" : ImGui::GetKeyName((ImGuiKey)*key);
        const ImVec2 ns = CalcTextScaled(name, th.FontSizeSmall);
        AddTextScaled(draw, ImVec2(boxPos.x + (boxW - ns.x) * 0.5f, boxPos.y + (h - ns.y) * 0.5f),
                      a.capturing ? th.OnAccent : th.TextMuted, name, th.FontSizeSmall);
        return changed;
    }
}
