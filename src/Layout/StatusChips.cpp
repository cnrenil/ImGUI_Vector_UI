#include "ImGuiVectorUI/Layout/StatusChips.h"

#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>
namespace IVUI
{
    static void DrawChips(ImVec2 anchorTopRight, const char* const* labels, int count)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float pad = th.HudPadding * S;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float y = anchorTopRight.y;
        for (int i = 0; i < count; ++i)
        {
            const char* name = labels[i];
            const float t = Toggle01(ImHashStr(name), true, 10.0f, EASE_OUT_CUBIC); // fade + slide in
            const ImVec2 ts = CalcTextScaled(name, th.FontSizeSmall);
            const float w = ts.x + pad * 2.5f;
            const float h = ts.y + pad;
            const float slide = (1.0f - t) * pad * 2.0f;
            const float rx = anchorTopRight.x + slide;
            const ImVec2 mn(rx - w, y);
            const ImVec2 mx(rx, y + h);
            dl->AddRectFilled(mn, mx, WithAlphaMul(th.Accent, th.HudPanelAlpha * t), PillRounding(h));
            dl->AddCircleFilled(ImVec2(mn.x + pad, y + h * 0.5f), 3.0f * S, WithAlphaMul(th.OnAccent, t));
            AddTextScaled(dl, ImVec2(mn.x + pad * 1.8f, y + (h - ts.y) * 0.5f), WithAlphaMul(th.OnAccent, t), name, th.FontSizeSmall);
            y += h + th.HudRowGap * S;
        }
    }

    static void DrawSideRail(ImVec2 anchorTopRight, const char* const* labels, int count)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float pad = th.HudPadding * S;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float widest = 0.0f;
        for (int i = 0; i < count; ++i)
            widest = ImMax(widest, CalcTextScaled(labels[i], th.FontSizeSmall).x);
        const float rowH = th.FontSizeSmall * S + pad;
        const float rowW = ImMax(th.HudMinWidth * S, widest + pad * 3.0f);
        const float railX = anchorTopRight.x - th.HudBorderThickness * S;
        const float top = anchorTopRight.y;
        const float bottom = top + rowH * count + th.HudRowGap * S * ImMax(count - 1, 0);
        dl->AddLine(ImVec2(railX, top), ImVec2(railX, bottom), th.Accent, ImMax(th.HudBorderThickness * S, 1.0f));

        float y = top;
        for (int i = 0; i < count; ++i)
        {
            const float t = Toggle01(ImHashStr(labels[i]), true, 10.0f, EASE_OUT_CUBIC);
            const ImVec2 ts = CalcTextScaled(labels[i], th.FontSizeSmall);
            const float slide = (1.0f - t) * pad * 1.8f;
            const ImVec2 mn(railX - rowW + slide, y);
            const ImVec2 mx(railX - pad * 0.6f + slide, y + rowH);
            dl->AddRectFilled(mn, mx, WithAlphaMul(th.Surface1, th.HudRowAlpha * t), th.RoundingControl * S);
            dl->AddCircleFilled(ImVec2(railX - pad + slide, y + rowH * 0.5f), 2.5f * S, WithAlphaMul(th.Accent, t));
            AddTextScaled(dl, ImVec2(railX - pad * 1.6f - ts.x + slide, y + (rowH - ts.y) * 0.5f),
                          WithAlphaMul(th.Text, t), labels[i], th.FontSizeSmall);
            y += rowH + th.HudRowGap * S;
        }
    }

    static void DrawMarquee(ImVec2 anchorTopRight, const char* const* labels, int count)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float pad = th.HudPadding * S;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float widest = 0.0f;
        for (int i = 0; i < count; ++i)
            widest = ImMax(widest, CalcTextScaled(labels[i], th.FontSizeSmall).x);

        const float rowH = th.FontSizeSmall * S + pad;
        const float width = ImMax(th.HudMinWidth * S, widest + pad * 3.0f);
        const float height = rowH * count + th.HudRowGap * S * ImMax(count - 1, 0) + pad * 2.0f;
        const ImVec2 mn(anchorTopRight.x - width, anchorTopRight.y);
        const ImVec2 mx(anchorTopRight.x, anchorTopRight.y + height);

        // Hue follows the border length to create a continuous perimeter animation.
        const ImVec2 edge[5] = {
            ImVec2(mn.x + th.HudBorderThickness * S * 2.0f, mn.y + th.HudBorderThickness * S * 2.0f),
            ImVec2(mx.x - th.HudBorderThickness * S * 2.0f, mn.y + th.HudBorderThickness * S * 2.0f),
            ImVec2(mx.x - th.HudBorderThickness * S * 2.0f, mx.y - th.HudBorderThickness * S * 2.0f),
            ImVec2(mn.x + th.HudBorderThickness * S * 2.0f, mx.y - th.HudBorderThickness * S * 2.0f),
            ImVec2(mn.x + th.HudBorderThickness * S * 2.0f, mn.y + th.HudBorderThickness * S * 2.0f)
        };
        dl->AddRectFilled(mn, mx, WithAlphaMul(th.Surface0, th.HudPanelAlpha), th.RoundingControl * S);
        const int segmentsPerEdge = 20;
        const float phase = fmodf((float)ImGui::GetTime() * th.HudRainbowSpeed, 1.0f);
        for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex)
        {
            const ImVec2 a = edge[edgeIndex];
            const ImVec2 b = edge[edgeIndex + 1];
            for (int i = 0; i < segmentsPerEdge; ++i)
            {
                const float p0 = (float)(edgeIndex * segmentsPerEdge + i) / (segmentsPerEdge * 4);
                float r, g, bcol;
                ImGui::ColorConvertHSVtoRGB(fmodf(phase + p0, 1.0f), 0.86f, 1.0f, r, g, bcol);
                const ImU32 col = IM_COL32((int)(r * 255.0f), (int)(g * 255.0f), (int)(bcol * 255.0f), 255);
                const ImVec2 pnt0 = ImLerp(a, b, (float)i / segmentsPerEdge);
                const ImVec2 pnt1 = ImLerp(a, b, (float)(i + 1) / segmentsPerEdge);
                dl->AddLine(pnt0, pnt1, WithAlphaMul(col, th.HudRainbowGlow), 5.0f * S);
                dl->AddLine(pnt0, pnt1, col, ImMax(th.HudBorderThickness * S, 1.0f));
            }
        }

        float y = mn.y + pad;
        for (int i = 0; i < count; ++i)
        {
            const float t = Toggle01(ImHashStr(labels[i]), true, 10.0f, EASE_OUT_CUBIC);
            const ImVec2 ts = CalcTextScaled(labels[i], th.FontSizeSmall);
            const float slide = (1.0f - t) * pad * 1.4f;
            dl->AddCircleFilled(ImVec2(mn.x + pad + slide, y + rowH * 0.5f), 2.5f * S, WithAlphaMul(th.Accent, t));
            AddTextScaled(dl, ImVec2(mn.x + pad * 2.0f + slide, y + (rowH - ts.y) * 0.5f),
                          WithAlphaMul(th.Text, t), labels[i], th.FontSizeSmall);
            y += rowH + th.HudRowGap * S;
        }
    }

    static void DrawGlassStack(ImVec2 anchorTopRight, const char* const* labels, int count)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float pad = th.HudPadding * S;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float widest = 0.0f;
        for (int i = 0; i < count; ++i)
            widest = ImMax(widest, CalcTextScaled(labels[i], th.FontSizeSmall).x);
        const float h = th.FontSizeSmall * S + pad;
        const float w = ImMax(th.HudMinWidth * S, widest + pad * 3.2f);
        float y = anchorTopRight.y;
        for (int i = 0; i < count; ++i)
        {
            const float t = Toggle01(ImHashStr(labels[i]), true, 10.0f, EASE_OUT_CUBIC);
            const float slide = (1.0f - t) * pad * 1.5f;
            const ImVec2 mn(anchorTopRight.x - w + slide, y);
            const ImVec2 mx(anchorTopRight.x + slide, y + h);
            const float round = PillRounding(h);
            const float glassAlpha = ImClamp(th.SurfaceAlpha * th.HudPanelAlpha, 0.0f, 0.65f);
            dl->AddRectFilled(mn, mx, WithAlphaMul(th.Surface1, glassAlpha * th.HudRowAlpha * t), round);
            dl->AddRect(mn, mx, WithAlphaMul(th.Border, t), round, 0, ImMax(th.HudBorderThickness * S, 1.0f));
            Sheen(dl, mn, mx, round, th.Sheen * t);
            dl->AddCircleFilled(ImVec2(mn.x + pad, y + h * 0.5f), 3.0f * S, WithAlphaMul(th.Accent, t));
            const ImVec2 ts = CalcTextScaled(labels[i], th.FontSizeSmall);
            AddTextScaled(dl, ImVec2(mn.x + pad * 1.8f, y + (h - ts.y) * 0.5f), WithAlphaMul(th.Text, t), labels[i], th.FontSizeSmall);
            y += h + th.HudRowGap * S;
        }
    }

    void StatusList(ImVec2 anchorTopRight, const char* const* labels, int count, const StatusListOptions& options)
    {
        if (count <= 0 || !labels) return;
        StatusListStyle style = options.style;
        if (style == StatusListStyle::ThemeDefault)
        {
            switch (GetTheme().HudLayout)
            {
            case HUD_SideRail:   style = StatusListStyle::SideRail; break;
            case HUD_RainbowFrame: style = StatusListStyle::Marquee; break;
            case HUD_GlassStack: style = StatusListStyle::GlassStack; break;
            default:             style = StatusListStyle::Chips; break;
            }
        }
        switch (style)
        {
        case StatusListStyle::SideRail:   DrawSideRail(anchorTopRight, labels, count); break;
        case StatusListStyle::Marquee:    DrawMarquee(anchorTopRight, labels, count); break;
        case StatusListStyle::GlassStack: DrawGlassStack(anchorTopRight, labels, count); break;
        default:                           DrawChips(anchorTopRight, labels, count); break;
        }
    }

    void StatusChips(ImVec2 anchorTopRight, const char* const* labels, int count)
    {
        DrawChips(anchorTopRight, labels, count);
    }

    void StatusChips(ImVec2 anchorTopRight, std::initializer_list<const char*> activeLabels)
    {
        StatusChips(anchorTopRight, activeLabels.begin(), (int)activeLabels.size());
    }
}
