#include "ImGuiVectorUI/Widgets/Toast.h"

#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <cmath>

namespace IVUI
{
    struct ToastItem
    {
        ToastType   type;
        std::string title;
        std::string body;
        float       duration;     // total visible seconds (excludes fades)
        float       age = 0.0f;   // seconds elapsed (paused while hovered)
        float       vis = 0.0f;   // 0..1 eased presence (drives slide + fade)
        bool        alive = true; // cleared once fully hidden
    };

    static std::vector<ToastItem> g_toasts;

    static ImU32 TypeColor(ToastType t)
    {
        switch (t)
        {
        case ToastType::Success: return IM_COL32(64, 200, 120, 255);
        case ToastType::Warning: return IM_COL32(240, 190, 70, 255);
        case ToastType::Error:   return IM_COL32(235, 90, 90, 255);
        case ToastType::Info:
        default:                 return GetTheme().Accent;
        }
    }

    static const char* TypeGlyph(ToastType t)
    {
        switch (t)
        {
        case ToastType::Success: return "\xE2\x9C\x93"; // ✓
        case ToastType::Warning: return "!";
        case ToastType::Error:   return "\xC3\x97";     // ×
        case ToastType::Info:
        default:                 return "i";
        }
    }

    void Toast(ToastType type, const char* title, const char* body, float seconds)
    {
        ToastItem it;
        it.type = type;
        it.title = title ? title : "";
        it.body = body ? body : "";
        it.duration = seconds > 0.1f ? seconds : 0.1f;
        g_toasts.push_back(std::move(it));
    }

    void ClearToasts()
    {
        g_toasts.clear();
    }

    void RenderToasts()
    {
        if (g_toasts.empty())
            return;

        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float dt = ImClamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float margin = 18.0f * S;
        const float cardW = 300.0f * S;
        const float padX = 14.0f * S;
        const float padY = 11.0f * S;
        const float iconR = 11.0f * S;
        const float gap = 10.0f * S;   // between icon and text
        const float spacing = 10.0f * S; // between stacked cards
        const float rightX = vp->Pos.x + vp->Size.x - margin;
        float y = vp->Pos.y + vp->Size.y - margin; // bottom anchor, grows upward

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // iOS "Dynamic Island": dark capsules that pop in from the top center and
        // expand, stacked downward (newest on top).
        if (th.DynamicIslandToast)
        {
            const float cx = vp->Pos.x + vp->Size.x * 0.5f;
            float iy = vp->Pos.y + margin;
            for (int i = (int)g_toasts.size() - 1; i >= 0; --i)
            {
                ToastItem& it = g_toasts[i];
                const ImVec2 titleSz = CalcTextScaled(it.title.c_str(), th.FontSizeLabel, GetTitleFont());
                const bool hasBody = !it.body.empty();
                const ImVec2 bodySz = hasBody ? CalcTextScaled(it.body.c_str(), th.FontSizeSmall) : ImVec2(0, 0);
                const float textW = ImMax(titleSz.x, bodySz.x);
                const float textH = titleSz.y + (hasBody ? (3.0f * S + bodySz.y) : 0.0f);
                const float capH = ImMax(iconR * 2.0f, textH) + padY * 2.0f;
                const float capW = padX * 2.5f + iconR * 2.0f + gap + textW;

                const ImVec2 hitMin(cx - capW * 0.5f, iy);
                const ImVec2 hitMax(cx + capW * 0.5f, iy + capH);
                const bool hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    it.alive = false;
                if (!hovered && it.alive)
                    it.age += dt;
                if (it.age >= it.duration)
                    it.alive = false;
                it.vis = ImLerp(it.vis, it.alive ? 1.0f : 0.0f, 1.0f - expf(-11.0f * dt));

                const float e = EaseValue(0.0f, 1.0f, ImClamp(it.vis, 0.0f, 1.0f), EASE_OUT_CUBIC);
                const float alpha = ImClamp(it.vis, 0.0f, 1.0f);
                const float sc = 0.7f + 0.3f * e;         // expand from a small pill
                const float w = capW * sc, h = capH * sc;
                const float drop = (1.0f - e) * 14.0f * S;
                const ImVec2 cmin(cx - w * 0.5f, iy + drop);
                const ImVec2 cmax(cx + w * 0.5f, iy + drop + h);
                const float round = h * 0.5f;             // full capsule
                const ImU32 tc = TypeColor(it.type);

                dl->AddRectFilled(ImVec2(cmin.x, cmin.y + 5.0f * S), ImVec2(cmax.x, cmax.y + 5.0f * S),
                                  WithAlphaMul(IM_COL32(0, 0, 0, 150), alpha), round);
                dl->AddRectFilled(cmin, cmax, WithAlphaMul(IM_COL32(12, 12, 15, 240), alpha), round);
                Sheen(dl, cmin, cmax, round, 0.5f * alpha);

                const ImVec2 iconC(cmin.x + padX + iconR, (cmin.y + cmax.y) * 0.5f);
                dl->AddCircleFilled(iconC, iconR * sc, WithAlphaMul(tc, alpha));
                const char* glyph = TypeGlyph(it.type);
                const ImVec2 gsz = CalcTextScaled(glyph, th.FontSizeSmall, GetTitleFont());
                AddTextScaled(dl, ImVec2(iconC.x - gsz.x * 0.5f, iconC.y - gsz.y * 0.5f),
                              WithAlphaMul(IM_COL32(12, 12, 15, 255), alpha), glyph, th.FontSizeSmall, GetTitleFont());

                // Text fades in as the capsule finishes expanding.
                const float txtA = alpha * ImClamp((e - 0.5f) * 2.0f, 0.0f, 1.0f);
                const float textX = iconC.x + iconR + gap;
                float ty = cmin.y + (h - textH) * 0.5f;
                AddTextScaled(dl, ImVec2(textX, ty), WithAlphaMul(IM_COL32(245, 245, 248, 255), txtA), it.title.c_str(), th.FontSizeLabel, GetTitleFont());
                if (hasBody)
                {
                    ty += titleSz.y + 3.0f * S;
                    AddTextScaled(dl, ImVec2(textX, ty), WithAlphaMul(IM_COL32(182, 184, 192, 255), txtA), it.body.c_str(), th.FontSizeSmall);
                }
                iy = cmax.y + spacing;
            }
            for (int i = (int)g_toasts.size() - 1; i >= 0; --i)
                if (!g_toasts[i].alive && g_toasts[i].vis < 0.02f)
                    g_toasts.erase(g_toasts.begin() + i);
            return;
        }

        // Newest nearest the corner: walk from the end (newest) upward.
        for (int i = (int)g_toasts.size() - 1; i >= 0; --i)
        {
            ToastItem& it = g_toasts[i];

            // Card height: title line + optional body line.
            const ImVec2 titleSz = CalcTextScaled(it.title.c_str(), th.FontSizeLabel, GetTitleFont());
            const bool hasBody = !it.body.empty();
            const ImVec2 bodySz = hasBody ? CalcTextScaled(it.body.c_str(), th.FontSizeSmall) : ImVec2(0, 0);
            const float textH = titleSz.y + (hasBody ? (4.0f * S + bodySz.y) : 0.0f);
            const float cardH = ImMax(iconR * 2.0f, textH) + padY * 2.0f;

            // Presence easing (slide-in from the right + fade). Countdown pauses on hover.
            const ImVec2 cardMinFull(rightX - cardW, y - cardH);
            const ImVec2 cardMaxFull(rightX, y);
            const bool hovered = ImGui::IsMouseHoveringRect(cardMinFull, cardMaxFull);
            const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            if (clicked)
                it.alive = false;
            if (!hovered && it.alive)
                it.age += dt;
            if (it.age >= it.duration)
                it.alive = false;

            const float target = it.alive ? 1.0f : 0.0f;
            it.vis = ImLerp(it.vis, target, 1.0f - expf(-11.0f * dt));

            const float e = EaseValue(0.0f, 1.0f, ImClamp(it.vis, 0.0f, 1.0f), EASE_OUT_CUBIC);
            const float alpha = ImClamp(it.vis, 0.0f, 1.0f);
            const float slide = (1.0f - e) * 44.0f * S;
            const ImVec2 cmin(cardMinFull.x + slide, cardMinFull.y);
            const ImVec2 cmax(cardMaxFull.x + slide, cardMaxFull.y);

            const ImU32 tc = TypeColor(it.type);
            const float round = th.RoundingControl * S;

            // Soft drop shadow.
            dl->AddRectFilled(ImVec2(cmin.x, cmin.y + 4.0f * S), ImVec2(cmax.x, cmax.y + 4.0f * S),
                              WithAlphaMul(IM_COL32(0, 0, 0, 130), alpha), round);
            // Card body + border.
            dl->AddRectFilled(cmin, cmax, WithAlphaMul(th.Surface1, 0.98f * alpha), round);
            dl->AddRect(cmin, cmax, WithAlphaMul(th.Border, alpha), round, 0, th.BorderThickness * S);
            // Left type accent bar.
            dl->AddRectFilled(cmin, ImVec2(cmin.x + 4.0f * S, cmax.y), WithAlphaMul(tc, alpha),
                              round, ImDrawFlags_RoundCornersLeft);

            // Type icon: filled disc + centered glyph.
            const ImVec2 iconC(cmin.x + padX + iconR, (cmin.y + cmax.y) * 0.5f);
            dl->AddCircleFilled(iconC, iconR, WithAlphaMul(tc, alpha));
            const char* glyph = TypeGlyph(it.type);
            const ImVec2 gsz = CalcTextScaled(glyph, th.FontSizeSmall, GetTitleFont());
            AddTextScaled(dl, ImVec2(iconC.x - gsz.x * 0.5f, iconC.y - gsz.y * 0.5f),
                          WithAlphaMul(IM_COL32(20, 22, 26, 255), alpha), glyph, th.FontSizeSmall, GetTitleFont());

            // Text block.
            const float textX = cmin.x + padX + iconR * 2.0f + gap;
            float ty = cmin.y + padY;
            AddTextScaled(dl, ImVec2(textX, ty), WithAlphaMul(th.Text, alpha), it.title.c_str(), th.FontSizeLabel, GetTitleFont());
            if (hasBody)
            {
                ty += titleSz.y + 4.0f * S;
                AddTextScaled(dl, ImVec2(textX, ty), WithAlphaMul(th.TextMuted, alpha), it.body.c_str(), th.FontSizeSmall);
            }

            // Bottom countdown bar (shrinks as time runs out).
            const float remain = ImClamp(1.0f - it.age / it.duration, 0.0f, 1.0f);
            const float barY = cmax.y - 2.5f * S;
            dl->AddLine(ImVec2(cmin.x + round, barY), ImVec2(cmin.x + round + (cardW - round * 2.0f) * remain, barY),
                        WithAlphaMul(tc, 0.85f * alpha), 2.5f * S);

            y = cmin.y - spacing;
        }

        // Reap fully-hidden toasts (collect-then-erase; safe for the next frame).
        for (int i = (int)g_toasts.size() - 1; i >= 0; --i)
            if (!g_toasts[i].alive && g_toasts[i].vis < 0.02f)
                g_toasts.erase(g_toasts.begin() + i);
    }
}
