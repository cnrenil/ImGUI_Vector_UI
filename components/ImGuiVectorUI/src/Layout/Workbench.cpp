#include "ImGuiVectorUI/Layout/Workbench.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"

#include <algorithm>
#include <cmath>

namespace IVUI
{
    namespace
    {
        float Positive(float value, float fallback)
        {
            return std::isfinite(value) && value > 0.0f ? value : fallback;
        }

        void BandText(ImDrawList* draw, ImVec2 position, ImVec2 size,
                      const char* text, ImU32 color, float fontSize, float padding)
        {
            if (!text || size.x <= padding * 2 || size.y <= 0) return;
            const float height = fontSize * GetTheme().Scale;
            const ImVec4 clip(position.x + padding, position.y,
                              position.x + size.x - padding, position.y + size.y);
            draw->AddText(GetRegularFont(), height,
                ImVec2(clip.x, position.y + (size.y - height) * 0.5f),
                color, text, nullptr, 0.0f, &clip);
        }
    }

    void Workbench(const char* id, const WorkbenchOptions& options,
                   const std::function<void(bool)>& navigation,
                   const std::function<void()>& body)
    {
        const Theme& theme = GetTheme();
        const float scale = Positive(theme.Scale, 1.0f);
        const float padding = std::max(theme.PanelPadding.x * scale, 0.0f);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 size(Positive(options.size.x, std::max(available.x, 1.0f)),
                          Positive(options.size.y, std::max(available.y, 1.0f)));
        const bool compact = size.x < Positive(options.compactBreakpoint, 640.0f) * scale;
        const float header = std::min((theme.ControlHeight + 14.0f) * scale, size.y);
        const float footer = options.status
            ? std::min(28.0f * scale, std::max(size.y - header - 1.0f, 0.0f)) : 0.0f;
        const float middle = std::max(size.y - header - footer, 0.0f);
        const float navWidth = navigation && !compact
            ? std::min(Positive(options.sidebarWidth, 192.0f) * scale, size.x * 0.4f) : 0.0f;
        const float navHeight = navigation && compact
            ? std::min((theme.ControlHeight + 16.0f) * scale, std::max(middle - 1.0f, 0.0f)) : 0.0f;
        const float rounding = std::max(theme.RoundingPanel * scale, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        const bool visible = ImGui::BeginChild(id, size, ImGuiChildFlags_None,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        if (visible)
        {
            const ImVec2 origin = ImGui::GetWindowPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 extent(origin.x + size.x, origin.y + size.y);
            if (!options.transparentSurface)
                draw->AddRectFilled(origin, extent, theme.Surface0, rounding);
            draw->AddRectFilled(
                origin, ImVec2(origin.x + size.x, origin.y + header),
                options.transparentSurface
                    ? WithAlphaMul(theme.Surface1, theme.PanelBodyAlpha * 0.12f)
                    : theme.Surface1,
                rounding, ImDrawFlags_RoundCornersTop);
            draw->AddLine(ImVec2(origin.x, origin.y + header),
                          ImVec2(origin.x + size.x, origin.y + header), theme.BorderMuted);
            BandText(draw, origin, ImVec2(size.x, header), options.title,
                     theme.Text, theme.FontSizeTitle, padding);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, 8.0f * scale));
            if (navigation && middle > 1.0f)
            {
                const ImVec2 navSize = compact ? ImVec2(size.x, navHeight) : ImVec2(navWidth, middle);
                if (navSize.x > 0 && navSize.y > 0)
                {
                    ImGui::SetCursorPos(ImVec2(0, header));
                    if (ImGui::BeginChild("##navigation", navSize,
                                          ImGuiChildFlags_AlwaysUseWindowPadding,
                                          ImGuiWindowFlags_NoBackground))
                        navigation(compact);
                    ImGui::EndChild();
                }
            }
            const ImVec2 contentSize(size.x - navWidth, middle - navHeight);
            if (contentSize.x > 0 && contentSize.y > 0)
            {
                ImGui::SetCursorPos(ImVec2(navWidth, header + navHeight));
                if (ImGui::BeginChild("##body", contentSize,
                                      ImGuiChildFlags_AlwaysUseWindowPadding,
                                      ImGuiWindowFlags_NoBackground))
                {
                    if (body) body();
                }
                ImGui::EndChild();
            }
            ImGui::PopStyleVar();
            if (navWidth > 0)
                draw->AddLine(ImVec2(origin.x + navWidth, origin.y + header),
                    ImVec2(origin.x + navWidth, origin.y + size.y - footer), theme.BorderMuted);
            if (footer > 0)
            {
                const ImVec2 start(origin.x, origin.y + size.y - footer);
                draw->AddRectFilled(
                    start, extent,
                    options.transparentSurface
                        ? WithAlphaMul(theme.Surface1, theme.PanelBodyAlpha * 0.18f)
                        : theme.Surface1,
                    rounding, ImDrawFlags_RoundCornersBottom);
                draw->AddLine(start, ImVec2(origin.x + size.x, start.y), theme.BorderMuted);
                BandText(draw, start, ImVec2(size.x, footer), options.status,
                         theme.TextMuted, theme.FontSizeSmall, padding);
            }
        }
        ImGui::EndChild();
    }
}
