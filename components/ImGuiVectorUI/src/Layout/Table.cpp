#include "ImGuiVectorUI/Layout/Table.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui.h"

namespace IVUI
{
    static const int kColorCount = 9;

    bool BeginTable(const char* id, int columns, ImVec2 outerSize, ImGuiTableFlags extraFlags)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, th.Surface2);
        ImGui::PushStyleColor(ImGuiCol_TableRowBg, th.Surface0);
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, th.Surface1);
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, th.BorderMuted);
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, th.Border);
        ImGui::PushStyleColor(ImGuiCol_Text, th.Text);
        ImGui::PushStyleColor(ImGuiCol_Header, th.Accent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, th.Surface2);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, th.AccentActive);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f * S, 6.0f * S));

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                                      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | extraFlags;
        const bool open = ImGui::BeginTable(id, columns, flags, outerSize);
        if (!open)
        {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(kColorCount);
        }
        return open;
    }

    void EndTable()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(kColorCount);
    }
}
