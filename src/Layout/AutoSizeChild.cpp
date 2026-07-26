#include "ImGuiVectorUI/Layout/AutoSizeChild.h"

#include "imgui_internal.h"

namespace IVUI
{
    float AnimatedChild(const char* id, ImVec2 outerSize, float innerWidth, const std::function<void()>& body)
    {
        ImGui::PushID(id);

        // BeginChild treats a size component of 0 as "fill available region"; during
        // the reveal animation the height passes through 0, which would make the child
        // balloon to fill the parent (flicker + layout jumps). Never pass 0.
        outerSize.x = ImMax(outerSize.x, 1.0f);
        outerSize.y = ImMax(outerSize.y, 1.0f);

        // Outer clips to the (animated) visible height; inner auto-resizes to content.
        ImGui::BeginChild("ivui_outer", outerSize, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::BeginChild("ivui_inner", ImVec2(innerWidth, 0.0f), ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoBackground);
        if (body)
            body();
        ImGui::EndChild();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        const float measured = mx.y - mn.y;
        ImGui::EndChild();

        ImGui::PopID();
        return measured;
    }
}
