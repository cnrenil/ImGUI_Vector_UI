/** @file Motion.h
 *  @brief Frame-rate-independent, ID-keyed animation helpers.
 *
 *  Animation state is owned by the current IVUI context and is collected when
 *  the associated ID is no longer submitted.
 */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Easing.h"

#include <memory>

namespace IVUI
{
    struct MotionSettings
    {
        float speed = 1.0f; ///< Nonnegative playback rate; zero pauses motion.
        bool reducedMotion = false; ///< Settle transitions immediately.
    };

    /** Exponential response, invariant under frame subdivision. */
    float MotionResponse(float speed = 12.0f);
    /** Constant-rate progress with the context's playback/accessibility policy. */
    float AdvanceProgress(float value, float target, float speed);

#ifdef IVUI_INTERNAL_API
    void SetMotionSettings(const MotionSettings& settings);
    MotionSettings GetMotionSettings();
#endif

    struct ItemMotionStyle
    {
        float enterDuration = 0.20f;
        float exitDuration = 0.16f;
        EasingType enterEasing = EASE_OUT_CUBIC;
        EasingType exitEasing = EASE_IN_CUBIC;
        ImVec2 offset{0.0f, 8.0f};
        float scale = 0.98f;
        float minimumAlpha = 0.0f;
        bool animateInitial = true;

        static constexpr ItemMotionStyle Fade(float duration = 0.18f) noexcept
        {
            ItemMotionStyle style{};
            style.enterDuration = duration;
            style.exitDuration = duration;
            style.offset = {};
            style.scale = 1.0f;
            return style;
        }

        static constexpr ItemMotionStyle FadeSlide(
            ImVec2 offset = {0.0f, 8.0f}, float duration = 0.20f) noexcept
        {
            ItemMotionStyle style{};
            style.enterDuration = duration;
            style.exitDuration = duration;
            style.offset = offset;
            return style;
        }
    };

    /** Applies visual alpha/translation/scale to vertices emitted by one item.
     *  When combining with ItemEffectScope, create the effect scope first so this
     *  motion scope ends and transforms the source before the effect captures it.
     */
    class ItemMotionScope
    {
    public:
        ItemMotionScope() noexcept = default;
        ~ItemMotionScope();
        ItemMotionScope(ItemMotionScope&& other) noexcept;
        ItemMotionScope& operator=(ItemMotionScope&& other) noexcept;
        ItemMotionScope(const ItemMotionScope&) = delete;
        ItemMotionScope& operator=(const ItemMotionScope&) = delete;

        explicit operator bool() const noexcept { return shouldDraw_; }
        float Progress() const noexcept { return progress_; }
        void End() noexcept;

    private:
        friend ItemMotionScope BeginItemMotion(ImDrawList*, ImGuiID, bool,
                                                const ItemMotionStyle&);
        ImDrawList* drawList_ = nullptr;
        int vertexStart_ = 0;
        ImVec2 offset_{};
        float scale_ = 1.0f;
        float alpha_ = 1.0f;
        float progress_ = 1.0f;
        unsigned frame_ = 0;
        std::weak_ptr<void> contextLifetime_;
        void* context_ = nullptr;
        bool active_ = false;
        bool shouldDraw_ = false;
    };

    /** @brief Smooths a scalar toward @p target, seeding it on first use. */
    float Approach(ImGuiID id, float target, float speed = 12.0f);
    float Approach(const char* label, float target, float speed = 12.0f);

    /** @brief Returns eased progress in [0, 1] toward @p on. */
    float Toggle01(ImGuiID id, bool on, float speed = 8.0f, EasingType e = EASE_IN_OUT_CIRC);
    float Toggle01(const char* label, bool on, float speed = 8.0f, EasingType e = EASE_IN_OUT_CIRC);

    /** @brief Smooths a packed color toward @p target, seeding it on first use. */
    ImU32 ColorApproach(ImGuiID id, ImU32 target, float speed = 10.0f, EasingType e = EASE_OUT_QUAD);
    ImU32 ColorApproach(const char* label, ImU32 target, float speed = 10.0f, EasingType e = EASE_OUT_QUAD);

    /** @brief Restarts a 0-to-1 transition when @p key changes. */
    float Transition(ImGuiID id, int key, float speed = 6.0f, EasingType e = EASE_OUT_CUBIC);
    float Transition(const char* label, int key, float speed = 6.0f, EasingType e = EASE_OUT_CUBIC);

    /** @brief Multiplies the alpha channel of @p col by @p a. */
    ImU32 WithAlphaMul(ImU32 col, float a);

    ItemMotionScope BeginItemMotion(ImDrawList* drawList, ImGuiID id, bool visible,
                                    const ItemMotionStyle& style = {});
    ItemMotionScope BeginItemMotion(ImGuiID id, bool visible,
                                    const ItemMotionStyle& style = {});
    ItemMotionScope BeginItemMotion(const char* label, bool visible,
                                    const ItemMotionStyle& style = {});
}
