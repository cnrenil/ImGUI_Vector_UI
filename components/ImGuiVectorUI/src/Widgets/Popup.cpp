#include "ImGuiVectorUI/Widgets/Popup.h"

#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Widgets/Button.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace IVUI
{
    void OpenConfirm(const char* id)
    {
        ImGui::OpenPopup(id);
    }

    bool ConfirmPopup(const char* id, const char* message, const char* yes, const char* no)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImGui::PushStyleColor(ImGuiCol_PopupBg, th.Surface1);
        ImGui::PushStyleColor(ImGuiCol_Border, th.Border);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 120));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, th.RoundingPanel * S);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * S, 16.0f * S));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, th.BorderThickness * S);

        bool confirmed = false;
        if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 c = ImGui::GetCursorScreenPos();
            AddTextScaled(dl, c, th.Text, message, th.FontSizeLabel);
            const ImVec2 ms = CalcTextScaled(message, th.FontSizeLabel);
            ImGui::Dummy(ImVec2(ImMax(ms.x, 220.0f * S), ms.y + 14.0f * S));

            if (Button(yes, {.size = ImVec2(120.0f * S, 0.0f), .prominent = true}))
            {
                confirmed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (Button(no, {.size = ImVec2(120.0f * S, 0.0f)}))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
        return confirmed;
    }
}
