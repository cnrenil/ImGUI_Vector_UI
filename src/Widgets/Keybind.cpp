#include "ImGuiVectorUI/Widgets/Keybind.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace IVUI
{
    static bool KeyChip(const char* idstr, ImGuiID rowId, int slot, int* key, ImVec2 pos, ImVec2 size)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        Context* context = GetContext();
        if (context == nullptr)
            return false;
        const bool wasCapturing = (context->keyCaptureId == rowId && context->keyCaptureSlot == slot);
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::InvisibleButton(idstr, size, ImGuiButtonFlags_EnableNav))
        {
            context->keyCaptureId = rowId;
            context->keyCaptureSlot = slot;
        }
        const bool capturing = (context->keyCaptureId == rowId && context->keyCaptureSlot == slot);

        dl->AddRectFilled(pos, pos + size, capturing ? th.Accent : th.Surface2, th.RoundingControl * S);
        dl->AddRect(pos, pos + size, th.BorderMuted, th.RoundingControl * S, 0, th.BorderThickness * S);
        const char* name = capturing ? "..." : (*key == ImGuiKey_None ? "—" : ImGui::GetKeyName((ImGuiKey)*key));
        const ImVec2 ns = CalcTextScaled(name, th.FontSizeSmall);
        AddTextScaled(dl, ImVec2(pos.x + (size.x - ns.x) * 0.5f, pos.y + (size.y - ns.y) * 0.5f),
                      capturing ? th.OnAccent : th.Text, name, th.FontSizeSmall);

        bool changed = false;
        if (wasCapturing) // scan only after the initiating click frame (so the click isn't bound)
        {
            context->wantCaptureHotkey = true;
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
            {
                if (ImGui::IsKeyPressed((ImGuiKey)k, false))
                {
                    context->keyCaptureId = 0;
                    context->keyCaptureSlot = -1;
                    *key = (k == ImGuiKey_Escape) ? ImGuiKey_None : k;
                    changed = true;
                    break;
                }
            }
        }
        return changed;
    }

    static bool ModeSeg(int* mode, ImVec2 pos, ImVec2 size)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* labels[3] = { "T", "H", "A" };
        const float segW = size.x / 3.0f;
        dl->AddRectFilled(pos, pos + size, th.Surface2, th.RoundingControl * S);
        dl->AddRect(pos, pos + size, th.BorderMuted, th.RoundingControl * S, 0, th.BorderThickness * S);
        bool changed = false;
        for (int i = 0; i < 3; ++i)
        {
            const ImVec2 sp(pos.x + segW * i, pos.y);
            const ImVec2 se(pos.x + segW * (i + 1), pos.y + size.y);
            ImGui::SetCursorScreenPos(sp);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("seg", ImVec2(segW, size.y), ImGuiButtonFlags_EnableNav) && *mode != i)
            {
                *mode = i;
                changed = true;
            }
            ImGui::PopID();
            if (*mode == i)
                dl->AddRectFilled(sp, se, th.Accent, th.RoundingControl * S);
            const ImVec2 ts = CalcTextScaled(labels[i], th.FontSizeSmall);
            AddTextScaled(dl, ImVec2(sp.x + (segW - ts.x) * 0.5f, sp.y + (size.y - ts.y) * 0.5f),
                          *mode == i ? th.OnAccent : th.TextMuted, labels[i], th.FontSizeSmall);
        }
        return changed;
    }

    bool KeybindRow(const char* label, Keybind* kb)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const ImGuiID rowId = ImGui::GetID(label);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float h = th.ControlHeight * S * 0.7f;
        const float rightInset = 12.0f * S;
        const float chipW = 54.0f * S, modeW = 84.0f * S, gap = 6.0f * S;

        const ImVec2 ls = CalcTextScaled(label, th.FontSizeLabel);
        AddTextScaled(dl, ImVec2(cursor.x, cursor.y + (h - ls.y) * 0.5f), th.Text, label, th.FontSizeLabel);

        const float right = cursor.x + avail.x - rightInset;
        const ImVec2 chipS(chipW, h);
        const ImVec2 modeP(right - modeW, cursor.y), modeS(modeW, h);
        const ImVec2 k2P(modeP.x - gap - chipW, cursor.y);
        const ImVec2 k1P(k2P.x - gap - chipW, cursor.y);

        ImGui::PushID(label);
        bool changed = false;
        changed |= KeyChip("k1", rowId, 0, &kb->key1, k1P, chipS);
        changed |= KeyChip("k2", rowId, 1, &kb->key2, k2P, chipS);
        changed |= ModeSeg(&kb->mode, modeP, modeS);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(cursor);
        ImGui::Dummy(ImVec2(avail.x, h));
        return changed;
    }
}
