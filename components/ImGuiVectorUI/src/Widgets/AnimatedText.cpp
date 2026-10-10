#include "ImGuiVectorUI/Widgets/AnimatedText.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h" // UTF-8 and word-wrap helpers

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <vector>

namespace IVUI
{
    namespace
    {
        struct GlyphLayout
        {
            const char* begin = nullptr;
            const char* end = nullptr;
            ImVec2 position{};
            std::size_t index = 0;
        };

        struct TextLayout
        {
            std::vector<GlyphLayout> glyphs;
            float width = 0.0f;
            int lineCount = 1;
        };

        float SafeScale(const Theme& theme)
        {
            return std::isfinite(theme.Scale) && theme.Scale > 0.0f ? theme.Scale : 1.0f;
        }

        float UnitPhase(float value)
        {
            return value - std::floor(value);
        }

        ImU32 LerpColor(ImU32 start, ImU32 end, float amount)
        {
            const ImVec4 a = ImGui::ColorConvertU32ToFloat4(start);
            const ImVec4 b = ImGui::ColorConvertU32ToFloat4(end);
            amount = ImClamp(amount, 0.0f, 1.0f);
            return ImGui::ColorConvertFloat4ToU32(ImVec4(
                ImLerp(a.x, b.x, amount), ImLerp(a.y, b.y, amount),
                ImLerp(a.z, b.z, amount), ImLerp(a.w, b.w, amount)));
        }

        ImU32 RainbowColor(float hue, float alpha)
        {
            float red = 0.0f;
            float green = 0.0f;
            float blue = 0.0f;
            ImGui::ColorConvertHSVtoRGB(UnitPhase(hue), 0.85f, 1.0f, red, green, blue);
            return ImGui::ColorConvertFloat4ToU32(
                ImVec4(red, green, blue, ImClamp(alpha, 0.0f, 1.0f)));
        }

        float SmoothStep(float value)
        {
            value = ImClamp(value, 0.0f, 1.0f);
            return value * value * (3.0f - 2.0f * value);
        }

        bool Decode(const char* text, const char* end, unsigned int& codepoint,
                    const char*& next)
        {
            if (text >= end)
                return false;
            const int byteCount = ImTextCharFromUtf8(&codepoint, text, end);
            if (byteCount <= 0 || text + byteCount > end)
                return false;
            next = text + byteCount;
            return true;
        }

        float GlyphAdvance(ImFont* font, float size, const char* begin, const char* end)
        {
            return font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
        }

        void LayoutSegment(TextLayout& layout, ImFont* font, float fontSize,
                           const char* begin, const char* end, float wrapWidth,
                           float lineHeight, float& x, int& line)
        {
            const char* cursor = begin;
            while (cursor < end)
            {
                const char* segmentEnd = end;
                if (wrapWidth > 0.0f)
                {
                    segmentEnd = font->CalcWordWrapPosition(fontSize, cursor, end, wrapWidth);
                    if (segmentEnd <= cursor)
                    {
                        unsigned int codepoint = 0;
                        const char* next = cursor;
                        if (!Decode(cursor, end, codepoint, next))
                            break;
                        segmentEnd = next;
                    }
                }

                const char* text = cursor;
                while (text < segmentEnd)
                {
                    unsigned int codepoint = 0;
                    const char* next = text;
                        if (!Decode(text, segmentEnd, codepoint, next))
                            break;
                    if (codepoint != '\r' && codepoint != '\n')
                    {
                        const float advance = GlyphAdvance(font, fontSize, text, next);
                        if (wrapWidth > 0.0f && x > 0.0f && x + advance > wrapWidth)
                        {
                            x = 0.0f;
                            ++line;
                            layout.lineCount = ImMax(layout.lineCount, line + 1);
                        }
                        layout.glyphs.push_back({text, next, ImVec2(x, line * lineHeight),
                                                 layout.glyphs.size()});
                        x += advance;
                        layout.width = ImMax(layout.width, x);
                    }
                    text = next;
                }

                if (segmentEnd >= end)
                    break;

                const char* nextLine = ImTextCalcWordWrapNextLineStart(segmentEnd, end);
                if (nextLine <= cursor)
                    nextLine = segmentEnd;
                cursor = nextLine;
                x = 0.0f;
                ++line;
                layout.lineCount = ImMax(layout.lineCount, line + 1);
            }
    }

        TextLayout BuildLayout(ImFont* font, float fontSize, const char* begin,
                               const char* end, float wrapWidth, float lineHeight)
        {
            TextLayout layout;
            float x = 0.0f;
            int line = 0;
            const char* lineStart = begin;
            while (lineStart < end)
            {
                const char* lineEnd = lineStart;
                const char* newlineEnd = end;
                while (lineEnd < end)
                {
                    unsigned int codepoint = 0;
                    const char* next = lineEnd;
                    if (!Decode(lineEnd, end, codepoint, next))
                        break;
                    if (codepoint == '\n')
                    {
                        newlineEnd = next;
                        break;
                    }
                    lineEnd = next;
                }

                LayoutSegment(layout, font, fontSize, lineStart, lineEnd, wrapWidth,
                              lineHeight, x, line);
                if (newlineEnd == end)
                    break;

                lineStart = newlineEnd;
                x = 0.0f;
                ++line;
                layout.lineCount = ImMax(layout.lineCount, line + 1);
            }
            return layout;
        }

        double AnimationElapsed(ImGuiID itemId, TextAnimation animation)
        {
            Context* context = GetContext();
            const double now = context ? context->motionTime : ImGui::GetTime();
            if (!context)
                return now;

            constexpr ImGuiID timingSeed = 0x41545854u; // "ATXT"
            const ImGuiID timingId = ImHashData(&itemId, sizeof(itemId), timingSeed);
            AnimSlot& timing = context->anim.Get(timingId, context->frameCount);
            const int animationKey = static_cast<int>(animation) + 1;
            if (!timing.initialized || timing.opened != animationKey)
            {
                timing.t = static_cast<float>(now);
                timing.opened = animationKey;
                timing.initialized = true;
            }
            return ImMax(0.0, now - static_cast<double>(timing.t));
        }

        std::size_t RevealedGlyphs(std::size_t glyphCount, double elapsed,
                                   float duration, bool loop)
        {
            if (glyphCount == 0)
                return 0;
            if (!std::isfinite(duration) || duration <= 0.0f)
                return glyphCount;

            double progress = elapsed / static_cast<double>(duration);
            if (loop)
                progress -= std::floor(progress);
            else
                progress = ImClamp(progress, 0.0, 1.0);

            const double steps = static_cast<double>(glyphCount) + 1.0;
            return ImMin(glyphCount, static_cast<std::size_t>(progress * steps));
        }
    }

    void AnimatedText(const char* label, const AnimatedTextOptions& options)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;

        const Theme& theme = GetTheme();
        const TextAnimation animation = GetMotionSettings().reducedMotion
            ? static_cast<TextAnimation>(0) : options.animation;
        const float scale = SafeScale(theme);
        const float pointSize = std::isfinite(options.pointSize) && options.pointSize > 0.0f
            ? options.pointSize : theme.FontSizeLabel;
        const float fontSize = ImMax(pointSize * scale, 1.0f);
        const float lineSpacing = ImMax(0.0f, options.lineSpacing) * scale;
        const float lineHeight = fontSize + lineSpacing;
        ImFont* font = options.font ? options.font : GetRegularFont();
        const char* textBegin = label ? label : "";
        const char* textEnd = VisibleEnd(textBegin);
        const ImGuiID itemId = window->GetID(textBegin);
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float requestedWrap = std::isfinite(options.wrapWidth) && options.wrapWidth > 0.0f
            ? options.wrapWidth * scale : availableWidth;
        const float wrapWidth = requestedWrap > 0.0f ? requestedWrap : FLT_MAX;
        const bool wave = HasTextAnimation(animation, TextAnimation::Wave);
        const float waveAmplitude = std::isfinite(options.waveAmplitude)
            ? std::fabs(options.waveAmplitude) * scale : 0.0f;
        const float wavePadding = wave ? waveAmplitude : 0.0f;
        const TextLayout layout = BuildLayout(font, fontSize, textBegin, textEnd,
                                               wrapWidth, lineHeight);
        const float textHeight = layout.lineCount > 0
            ? fontSize + (layout.lineCount - 1) * lineHeight : 0.0f;
        const ImVec2 textPosition(
            window->DC.CursorPos.x,
            window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset + wavePadding);
        const ImVec2 itemSize(layout.width, textHeight + wavePadding * 2.0f);
        const ImRect itemBounds(textPosition - ImVec2(0.0f, wavePadding),
                                textPosition + ImVec2(layout.width, textHeight + wavePadding));
        ImGui::ItemSize(itemSize, 0.0f);
        if (!ImGui::ItemAdd(itemBounds, itemId))
            return;

        ElementScope elementScope = BeginElement(window->DrawList, itemId, true, options.element);
        if (layout.glyphs.empty())
            return;

        const double elapsed = AnimationElapsed(itemId, animation);
        const float speed = std::isfinite(options.speed) ? options.speed : 1.0f;
        const float cycle = static_cast<float>(elapsed * static_cast<double>(speed));
        const ImU32 colorStart = options.colorStart != 0 ? options.colorStart : theme.Text;
        const ImU32 colorEnd = options.colorEnd != 0 ? options.colorEnd : theme.Accent;
        const float startAlpha = ImGui::ColorConvertU32ToFloat4(colorStart).w;
        const std::size_t revealCount = HasTextAnimation(animation, TextAnimation::Typewriter)
            ? RevealedGlyphs(layout.glyphs.size(), elapsed, options.typewriterDuration, options.loop)
            : layout.glyphs.size();
        const float fadeDuration = options.fadeDuration > 0.0f
            ? options.fadeDuration : options.typewriterDuration;

        for (const GlyphLayout& glyph : layout.glyphs)
        {
            const float fraction = layout.glyphs.size() > 1
                ? static_cast<float>(glyph.index) / static_cast<float>(layout.glyphs.size() - 1)
                : 0.0f;
            ImVec2 position = textPosition + glyph.position;
            ImU32 color = colorStart;

            if (HasTextAnimation(animation, TextAnimation::Rainbow))
            {
                color = RainbowColor(fraction * 0.9f + cycle * 0.15f, startAlpha);
            }
            else if (HasTextAnimation(animation, TextAnimation::GradientFlow))
            {
                color = LerpColor(colorStart, colorEnd, UnitPhase(fraction - cycle * 0.25f));
            }

            if (HasTextAnimation(animation, TextAnimation::ColorPulse))
            {
                const float pulse = 0.5f + 0.5f * std::sin(
                    (cycle + fraction * 0.35f) * 2.0f * IM_PI);
                color = LerpColor(color, colorEnd, 0.15f + pulse * 0.70f);
            }

            if (HasTextAnimation(animation, TextAnimation::Wave))
                position.y += std::sin((fraction + cycle * 0.25f) * 2.0f * IM_PI) * waveAmplitude;

            if (HasTextAnimation(animation, TextAnimation::Flicker))
            {
                const float flicker = 0.42f + 0.58f * (0.5f + 0.5f * std::sin(
                    cycle * 17.0f + static_cast<float>(glyph.index) * 12.9898f));
                color = WithAlphaMul(color, flicker);
            }

            if (HasTextAnimation(animation, TextAnimation::FadeIn))
            {
                const float duration = std::isfinite(fadeDuration) && fadeDuration > 0.0f
                    ? fadeDuration : 0.001f;
                const float local = static_cast<float>(elapsed / duration) -
                                    fraction * ImMax(0.0f, options.fadeStagger);
                color = WithAlphaMul(color, SmoothStep(local));
            }

            if (glyph.index < revealCount)
                window->DrawList->AddText(font, fontSize, position, color,
                                           glyph.begin, glyph.end);
        }
    }
}
