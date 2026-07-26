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

        const bool clicked = ImGui::InvisibleButton(label, size);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();

        // Ease toward hover/held state.
        const float target = held ? 1.0f : (hovered ? 0.6f : 0.0f);
        a.t = ImClamp(a.t + (target - a.t) * ImClamp(DeltaTime() * 12.0f, 0.0f, 1.0f), 0.0f, 1.0f);

        // Press inset (shrinks slightly while held).
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
        // Style-driven glow: prominent always reacts; plain buttons glow on hover.
        WidgetGlow(draw, mn, mx, th.Accent, bround, prominent ? ImMax(a.t, 0.35f) : a.t);
        draw->AddRectFilled(mn, mx, fill, bround);
        if (th.BorderThickness > 0.01f && (!prominent || th.FillGradient))
            draw->AddRect(mn, mx, prominent ? th.Border : th.BorderMuted, bround, 0, th.BorderThickness * S);
        Sheen(draw, mn, mx, bround, th.Sheen);

        const ImU32 textCol = prominent ? th.OnAccent : th.Text;
        AddTextScaled(draw, ImVec2(cursor.x + (size.x - textSize.x) * 0.5f, cursor.y + (size.y - textSize.y) * 0.5f),
                      textCol, label, th.FontSizeLabel);
        return clicked;
    }
}
