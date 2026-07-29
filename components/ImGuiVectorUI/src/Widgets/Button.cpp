#include "ImGuiVectorUI/Widgets/Button.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui_internal.h" // ImClamp, ImMax

namespace IVUI
{
    bool Button(const char* label, ImVec2 size, bool prominent)
    {
        const Theme& th = GetTheme();
        AnimSlot& a = Slot(label);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float S = th.Scale;

        const ImVec2 textSize = CalcTextScaled(label, th.FontSizeLabel);
        if (size.x <= 0.0f)
            size.x = textSize.x + 28.0f * S;
        if (size.y <= 0.0f)
            size.y = th.ControlHeight * S * 0.8f;
        size.x = ImMin(size.x, ImMax(avail.x, size.x));

        const bool clicked = ImGui::InvisibleButton(label, size, ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();

        const float target = held ? 1.0f : (hovered ? 0.6f : 0.0f);
        a.t = ImClamp(a.t + (target - a.t) * ImClamp(DeltaTime() * 12.0f, 0.0f, 1.0f), 0.0f, 1.0f);

        a.t2 = ImClamp(a.t2 + ((held ? 1.0f : 0.0f) - a.t2) * ImClamp(DeltaTime() * 20.0f, 0.0f, 1.0f), 0.0f, 1.0f);
        const float inset = EaseValue(0.0f, 2.5f * S, a.t2, EASE_OUT_QUAD);

        const ImU32 base = prominent ? th.Accent : th.Surface2;
        const ImU32 hot = prominent ? th.AccentHover : th.Border;
        ImU32 fill = EaseValueColor(ImColor(base), ImColor(hot), a.t, EASE_OUT_QUAD);
        if (!prominent)
            fill = WithAlphaMul(fill, th.SurfaceAlpha); // translucent in glass styles
        const ImVec2 mn = cursor + ImVec2(inset, inset);
        const ImVec2 mx = cursor + size - ImVec2(inset, inset);
        const float bround = PillRounding(size.y);
        const float glow = th.WidgetGlow * a.t;
        if (glow > 0.001f && th.WidgetGlowSpread > 0.0f)
        {
            constexpr int rings = 4;
            for (int ring = rings; ring >= 1; --ring)
            {
                const float fraction = static_cast<float>(ring) / rings;
                const float spread = th.WidgetGlowSpread * S * fraction;
                draw->AddRect(mn - ImVec2(spread, spread), mx + ImVec2(spread, spread),
                              WithAlphaMul(th.Accent, glow * (1.0f - fraction) * 0.35f),
                              bround + spread, 0, ImMax(1.0f, spread * 0.45f));
            }
        }
        const int vertexStart = draw->VtxBuffer.Size;
        draw->AddRectFilled(mn, mx, fill, bround);
        if (th.FillGradient)
        {
            const ImU32 top = prominent ? th.AccentHover : th.HoverTint;
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(
                draw, vertexStart, draw->VtxBuffer.Size, mn, ImVec2(mn.x, mx.y), top, fill);
        }
        if (th.BorderThickness > 0.01f && (!prominent || th.FillGradient))
            draw->AddRect(mn, mx, prominent ? th.Border : th.BorderMuted, bround, 0, th.BorderThickness * S);
        if (ImGui::IsItemFocused())
            draw->AddRect(mn - ImVec2(2.0f * S, 2.0f * S), mx + ImVec2(2.0f * S, 2.0f * S),
                          th.Accent, bround + 2.0f * S, 0, ImMax(1.5f * S, 1.0f));
        Sheen(draw, mn, mx, bround, th.Sheen);

        const ImU32 textCol = prominent ? th.OnAccent : th.Text;
        AddTextScaled(draw, ImVec2(cursor.x + (size.x - textSize.x) * 0.5f, cursor.y + (size.y - textSize.y) * 0.5f),
                      textCol, label, th.FontSizeLabel);
        return clicked;
    }
}
