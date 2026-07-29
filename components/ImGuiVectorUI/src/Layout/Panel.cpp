#include "ImGuiVectorUI/Layout/Panel.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/DragDrop.h"
#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Layout/AutoSizeChild.h"

#include "imgui_internal.h"

#include <string.h>

namespace IVUI
{
    static EffectColor ToEffectColor(ImU32 color)
    {
        const ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        return {value.x, value.y, value.z, value.w};
    }

    enum class TBIcon { Edit, Close };

    static bool TitleButton(const char* id, ImVec2 center, float r, TBIcon icon, bool active)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGui::SetCursorScreenPos(ImVec2(center.x - r, center.y - r));
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(r * 2.0f, r * 2.0f),
                                                    ImGuiButtonFlags_EnableNav);
        const bool hov = ImGui::IsItemHovered();
        if (active || hov)
            dl->AddCircleFilled(center, r, active ? th.Accent : th.Surface2);
        const ImU32 col = active ? th.OnAccent : (hov ? th.Text : th.TextMuted);
        const float g = r * 0.5f;
        const float thick = ImMax(1.6f * S, 1.0f);
        if (icon == TBIcon::Close)
        {
            dl->AddLine(ImVec2(center.x - g, center.y - g), ImVec2(center.x + g, center.y + g), col, thick);
            dl->AddLine(ImVec2(center.x - g, center.y + g), ImVec2(center.x + g, center.y - g), col, thick);
        }
        else // pencil
        {
            const ImVec2 tip(center.x - g, center.y + g);
            const ImVec2 top(center.x + g, center.y - g);
            dl->AddLine(tip, top, col, thick * 1.6f);
            dl->AddLine(ImVec2(top.x - g * 0.45f, top.y + g * 0.45f), top, col, thick);
            dl->AddCircleFilled(tip, thick * 0.9f, col);
        }
        return clicked;
    }

    static void PanelImpl(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body,
                          PanelHeader* hdr, ImGuiWindowFlags flags)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const float titleH = th.ControlHeight * S;
        const float pad = th.PanelPadding.x * S;

        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);
        ImGui::Begin(label, nullptr, flags);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 wpos = ImGui::GetWindowPos();
        ImGuiWindow* win = ImGui::GetCurrentWindow();

        // The ID suffix keeps animation state stable while the visible title changes.
        AnimSlot& a = SlotID(win->ID);

        a.direction = a.hidden ? -1.0f : 1.0f;
        a.t = ImClamp(a.t + DeltaTime() * 5.0f * a.direction, 0.0f, 1.0f);
        const float bodyH = EaseValue(0.0f, a.sizeY, a.t, EASE_OUT_QUAD);
        const float totalH = ImClamp(titleH + bodyH + pad, titleH, maxSize.y * S);
        ImGui::SetWindowSize(ImVec2(maxSize.x * S, totalH));
        const ImVec2 wsize = ImGui::GetWindowSize();

        // Submit glass before ImGui so it remains beneath panel content.
        const float pround = th.RoundingPanel * S;
        if (Context* context = GetContext(); context && context->effectRenderer)
        {
            BlurEffectParams blur;
            blur.bounds.min = {wpos.x, wpos.y};
            blur.bounds.max = {wpos.x + wsize.x, wpos.y + wsize.y};
            blur.sigma = th.BackdropBlurSigma;
            blur.rounding = pround;
            blur.opacity = 1.0f;
            SubmitEffect(*context->effectRenderer, EffectRequest::BackdropBlur(blur));

            GlassEffectParams g;
            g.bounds.min = {wpos.x, wpos.y};
            g.bounds.max = {wpos.x + wsize.x, wpos.y + wsize.y};
            g.rounding = pround;
            g.opacity = th.GlassOpacity;
            g.tint = ToEffectColor(th.GlassTint); g.tintStrength = th.GlassTintStrength;
            g.accent = ToEffectColor(th.Accent);
            g.blurSigma = th.BackdropBlurSigma;
            g.refraction = th.GlassRefraction * S;
            g.edgeWidth = th.GlassEdgeWidth * S;
            g.edgeHighlight = th.GlassEdgeHighlight;
            g.thickness = th.GlassThickness;
            g.shapePower = th.GlassShapePower;
            g.lensStrength = th.GlassLensStrength;
            g.lensBase = th.GlassLensBase;
            g.lensFalloff = th.GlassLensFalloff;
            g.lensBias = th.GlassLensBias;
            g.lensPower = th.GlassLensPower;
            g.noise = th.GlassNoise * S;
            g.glowStrength = th.PanelGlow; g.glowSpread = th.PanelGlowSpread * S;
            g.shadowStrength = th.PanelShadow; g.shadowSpread = th.PanelShadowSpread * S;
            SubmitEffect(*context->effectRenderer, EffectRequest::Glass(g));
        }
        else
            draw->AddRectFilled(wpos, wpos + wsize,
                                WithAlphaMul(th.GlassTint, th.GlassOpacity), pround);
        if (th.BorderThickness > 0.01f)
            draw->AddRect(wpos, wpos + wsize, th.Border, pround, 0, th.BorderThickness * S);

        const ImVec2 titleEnd = ImVec2(wpos.x + wsize.x, wpos.y + titleH);
        draw->AddRectFilled(wpos, titleEnd, WithAlphaMul(th.Surface1, th.PanelBodyAlpha * 0.10f), pround, ImDrawFlags_RoundCornersTop);
        Sheen(draw, wpos, titleEnd, pround, th.Sheen);
        draw->AddLine(ImVec2(wpos.x + pad, titleEnd.y), ImVec2(titleEnd.x - pad, titleEnd.y), th.Accent, ImMax(2.0f * S, 1.0f));
        Context* context = GetContext();
        ImGuiID& renameId = context->panelRenameId;
        bool& renameFocus = context->panelRenameFocus;
        const float rr = titleH * 0.28f;
        const int nBtns = hdr ? ((hdr->showEdit ? 1 : 0) + (hdr->showClose ? 1 : 0)) : 0;
        const float titleTextW = ImMax(wsize.x - pad * 2.0f - nBtns * (rr * 2.0f + 6.0f * S), 40.0f * S);
        const bool renaming = hdr && hdr->editBuf && renameId == win->ID;
        if (renaming)
        {
            ImGui::SetCursorScreenPos(ImVec2(wpos.x + pad, wpos.y + (titleH - ImGui::GetFrameHeight()) * 0.5f));
            ImGui::SetNextItemWidth(titleTextW);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, th.Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text, th.Text);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, th.RoundingControl * S);
            if (renameFocus) { ImGui::SetKeyboardFocusHere(); renameFocus = false; }
            const bool enter = ImGui::InputText("##prename", hdr->editBuf, (size_t)hdr->editBufSize,
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool committed = enter || ImGui::IsItemDeactivatedAfterEdit(); // Esc cancels (no commit)
            const bool closed = committed || ImGui::IsItemDeactivated();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            if (committed) hdr->nameChanged = true;
            if (closed) renameId = 0;
        }
        else
        {
            const ImVec2 titleSize = CalcTextScaled(label, th.FontSizeTitle, GetTitleFont());
            AddTextScaled(draw, ImVec2(wpos.x + pad, wpos.y + (titleH - titleSize.y) * 0.5f), th.Text, label, th.FontSizeTitle, GetTitleFont());
            if (hdr && hdr->editBuf &&
                ImGui::IsMouseHoveringRect(wpos, ImVec2(wpos.x + titleTextW, wpos.y + titleH)) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                renameId = win->ID;
                renameFocus = true;
                const char* end = VisibleEnd(label);
                size_t n = (size_t)(end - label);
                if (n >= (size_t)hdr->editBufSize) n = (size_t)hdr->editBufSize - 1;
                memcpy(hdr->editBuf, label, n);
                hdr->editBuf[n] = 0;
            }
        }

        if (hdr)
        {
            const float r = titleH * 0.28f;
            const float cy = wpos.y + titleH * 0.5f;
            float bx = titleEnd.x - pad - r;
            if (hdr->showClose)
            {
                hdr->closeClicked = TitleButton("##close", ImVec2(bx, cy), r, TBIcon::Close, false);
                bx -= r * 2.0f + 6.0f * S;
            }
            if (hdr->showEdit)
            {
                hdr->editClicked = TitleButton("##edit", ImVec2(bx, cy), r, TBIcon::Edit, hdr->editActive);
                bx -= r * 2.0f + 6.0f * S;
            }
        }

        if (ImGui::IsMouseHoveringRect(wpos, titleEnd) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            a.hidden = !a.hidden;

        ImGui::SetCursorPos(ImVec2(pad, titleH + pad * 0.5f));
        if (a.t > 0.001f)
        {
            const float measured = AnimatedChild(label, ImVec2(wsize.x - pad * 2.0f, bodyH), wsize.x - pad * 2.0f, body);
            a.sizeY = measured + pad;
        }

        ImGui::End();
    }

    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, ImGuiWindowFlags flags)
    {
        PanelImpl(label, pos, maxSize, body, nullptr, flags);
    }

    void Panel(const char* label, ImVec2 pos, ImVec2 maxSize, const std::function<void()>& body, PanelHeader* header, ImGuiWindowFlags flags)
    {
        if (header)
        {
            header->editClicked = false;
            header->closeClicked = false;
            header->nameChanged = false;
            header->droppedFeature = nullptr;
        }
        PanelImpl(label, pos, maxSize, body, header, flags);
    }
}
