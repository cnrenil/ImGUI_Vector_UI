#include "ImGuiVectorUI/Layout/ScrollRegion.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"

namespace IVUI
{
    void OverlayScrollbar()
    {
        const float scrollMax = ImGui::GetScrollMaxY();
        AnimSlot& a = SlotID(ImGui::GetID("##ivui_scroll"));
        const bool needed = scrollMax > 1.0f;
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        const bool show = needed && (hovered || a.capturing);
        a.direction = show ? 1.0f : -1.0f;
        a.t = ImClamp(a.t + DeltaTime() * 8.0f * a.direction, 0.0f, 1.0f);
        if (!needed)
            a.capturing = false;
        if (a.t <= 0.001f)
            return;

        const Theme& th = GetTheme();
        const float S = th.Scale;
        const ImVec2 wpos = ImGui::GetWindowPos();
        const ImVec2 wsize = ImGui::GetWindowSize();
        const float marginX = 0.0f; // flush to the right edge
        const float marginY = 2.0f * S;
        const float trackW = 5.0f * S;
        const float x1 = wpos.x + wsize.x - marginX;
        const float x0 = x1 - trackW;
        const float y0 = wpos.y + marginY;
        const float trackH = wsize.y - marginY * 2.0f;

        const float viewRatio = ImClamp(wsize.y / (wsize.y + scrollMax), 0.08f, 1.0f);
        const float thumbH = ImMax(trackH * viewRatio, 24.0f * S);
        float scrollY = ImGui::GetScrollY();
        float thumbY = y0 + (trackH - thumbH) * (scrollMax > 0.0f ? scrollY / scrollMax : 0.0f);

        const ImVec2 m = ImGui::GetMousePos();
        const bool overThumb = (m.x >= x0 - 4.0f * S && m.x <= x1 + 4.0f * S && m.y >= thumbY && m.y <= thumbY + thumbH);
        if (ImGui::IsMouseClicked(0) && overThumb && hovered)
            a.capturing = true;
        if (!ImGui::IsMouseDown(0))
            a.capturing = false;
        if (a.capturing && (trackH - thumbH) > 0.0f)
        {
            scrollY = ImClamp(scrollY + ImGui::GetIO().MouseDelta.y * (scrollMax / (trackH - thumbH)), 0.0f, scrollMax);
            ImGui::SetScrollY(scrollY);
            thumbY = y0 + (trackH - thumbH) * (scrollMax > 0.0f ? scrollY / scrollMax : 0.0f);
        }

        // Draw in window coordinates so the bar does not scroll with content.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float vis = EaseValue(0.0f, 1.0f, a.t, EASE_OUT_QUAD);
        const ImU32 trackCol = WithAlphaMul(th.Surface2, 0.6f * vis);
        const ImU32 thumbCol = WithAlphaMul((overThumb || a.capturing) ? th.Accent : th.TextMuted, vis);
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y0 + trackH), trackCol, trackW * 0.5f);
        dl->AddRectFilled(ImVec2(x0, thumbY), ImVec2(x1, thumbY + thumbH), thumbCol, trackW * 0.5f);
    }
}
