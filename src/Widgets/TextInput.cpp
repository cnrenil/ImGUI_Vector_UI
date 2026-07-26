#include "ImGuiVectorUI/Widgets/TextInput.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui.h"

namespace IVUI
{
    bool TextInput(const char* label, char* buf, size_t bufSize, const char* hint)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, th.Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, th.Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, th.Surface2);
        ImGui::PushStyleColor(ImGuiCol_Text, th.Text);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, th.TextDisabled);
        ImGui::PushStyleColor(ImGuiCol_Border, th.BorderMuted);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, th.RoundingControl * S);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, th.BorderThickness * S);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * S, 6.0f * S));

        ImGui::PushID(label);
        ImGui::SetNextItemWidth(-1.0f);
        bool changed;
        if (hint)
            changed = ImGui::InputTextWithHint("##ti", hint, buf, bufSize);
        else
            changed = ImGui::InputText("##ti", buf, bufSize);
        ImGui::PopID();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(6);
        return changed;
    }
}
