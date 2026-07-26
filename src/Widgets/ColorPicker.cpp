#include "ImGuiVectorUI/Widgets/ColorPicker.h"

#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"

#include "imgui_internal.h"

namespace IVUI
{
    bool ColorPicker(const char* label, ImVec4* col)
    {
        const Theme& th = GetTheme();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;
        const float rightInset = 12.0f * S;
        const float h = th.ControlHeight * S * 0.62f;
        const float sw = 46.0f * S;

        // Label (left).
        const ImVec2 ls = CalcTextScaled(label, th.FontSizeLabel);
        AddTextScaled(draw, ImVec2(cursor.x, cursor.y + (h - ls.y) * 0.5f), th.Text, label, th.FontSizeLabel);

        // Swatch (right-aligned to the shared right inset).
        const ImVec2 sMin(cursor.x + ImMax(avail.x - rightInset - sw, 0.0f), cursor.y);
        const ImVec2 sMax(sMin.x + sw, cursor.y + h);
        draw->AddRectFilled(sMin, sMax, ImGui::ColorConvertFloat4ToU32(ImVec4(col->x, col->y, col->z, 1.0f)), th.RoundingControl * S);
        draw->AddRect(sMin, sMax, th.BorderMuted, th.RoundingControl * S, 0, th.BorderThickness * S);

        ImGui::PushID(label);
        ImGui::SetCursorScreenPos(sMin);
        if (ImGui::InvisibleButton("swatch", ImVec2(sw, h)))
            ImGui::OpenPopup("pop");

        bool changed = false;
        if (ImGui::BeginPopup("pop"))
        {
            changed = ImGui::ColorPicker4("##pick", (float*)col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview);
            ImGui::EndPopup();
        }
        ImGui::PopID();
        return changed;
    }
}
