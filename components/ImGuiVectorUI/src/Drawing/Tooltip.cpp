#include "ImGuiVectorUI/Drawing/Tooltip.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui.h"

namespace IVUI
{
    void Tooltip(const char* text)
    {
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
            return;
        const Theme& th = GetTheme();
        ImGui::PushStyleColor(ImGuiCol_PopupBg, th.Surface1);
        ImGui::PushStyleColor(ImGuiCol_Border, th.Border);
        ImGui::PushStyleColor(ImGuiCol_Text, th.Text);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, th.RoundingControl * th.Scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * th.Scale, 7.0f * th.Scale));
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(text);
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
}
