/** @file AnimatedText.h @brief Individually animated UTF-8 text glyphs. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Element.h"

#include <cstdint>

namespace IVUI
{
    enum class TextAnimation
    {
        None        = 0,
        GradientFlow= 1u << 0,
        Rainbow     = 1u << 1,
        Wave        = 1u << 2,
        Flicker     = 1u << 3,
        Typewriter  = 1u << 4,
        ColorPulse  = 1u << 5,
        FadeIn      = 1u << 6
    };

    constexpr TextAnimation operator|(TextAnimation left, TextAnimation right) noexcept
    {
        return static_cast<TextAnimation>(static_cast<std::uint32_t>(left) |
                                          static_cast<std::uint32_t>(right));
    }

    constexpr bool HasTextAnimation(TextAnimation value, TextAnimation flag) noexcept
    {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
    }

    struct AnimatedTextOptions
    {
        TextAnimation animation = TextAnimation::None;
        /** Unscaled point size. Non-positive values select the theme label size. */
        float pointSize = 0.0f;
        /** Null selects the theme/context regular font. */
        ImFont* font = nullptr;
        /** Packed RGBA endpoints. Zero selects theme Text/Accent respectively. */
        ImU32 colorStart = 0;
        ImU32 colorEnd = 0;
        /** Animation cycles per second; negative values reverse cyclic effects. */
        float speed = 1.0f;
        /** Unscaled peak Wave displacement. */
        float waveAmplitude = 3.0f;
        /** Seconds required for a complete Typewriter reveal. */
        float typewriterDuration = 1.5f;
        /** Unscaled line spacing added between automatically wrapped lines. */
        float lineSpacing = 2.0f;
        /** Maximum line width in design pixels; zero uses the available content width. */
        float wrapWidth = 0.0f;
        /** Seconds for per-glyph FadeIn; zero uses the typewriter duration. */
        float fadeDuration = 0.0f;
        /** Fraction of the fade duration staggered across glyphs. */
        float fadeStagger = 0.35f;
        bool loop = true;
        ElementOptions element{ElementRole::Text};
    };

    /** Draws the visible portion of @p label as individually animated UTF-8 glyphs.
     *  Text is decoded and laid out by Unicode codepoint, with optional automatic word
     *  wrapping. The complete final text bounds are submitted even while Typewriter or
     *  FadeIn hides glyphs; an ImGui ID suffix remains part of item identity.
     */
    void AnimatedText(const char* label, const AnimatedTextOptions& options = {});
}
