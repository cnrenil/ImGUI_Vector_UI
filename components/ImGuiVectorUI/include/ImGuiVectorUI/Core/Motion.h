/** @file Motion.h
 *  @brief Frame-rate-independent, ID-keyed animation helpers.
 *
 *  Animation state is owned by the current IVUI context and is collected when
 *  the associated ID is no longer submitted.
 */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Easing.h"

namespace IVUI
{
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
}
