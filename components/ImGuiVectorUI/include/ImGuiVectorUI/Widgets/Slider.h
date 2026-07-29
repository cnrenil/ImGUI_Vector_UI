/** @file Slider.h @brief Animated continuous and discrete slider widgets. */
#pragma once

#include <initializer_list>

namespace IVUI
{
    /** @brief Draws a continuous float slider in the inclusive range [@p mn, @p mx].
     *  @param label Visible label and ImGui ID source.
     *  @param v Value to display and update.
     *  @param mn,mx Inclusive value bounds.
     *  @param fmt printf-style display format.
     *  @return True when @p v changed.
     */
    bool SliderFloat(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f");

    /** @brief Draws a stepped slider; @p v is constrained to [0, segments - 1].
     *  @param segments Number of discrete positions; must be positive.
     *  @return The resulting selected index.
     */
    int SliderSegments(const char* label, int* v, int segments, const char* fmt = "%d");

    /** @brief Draws a discrete slider; @p v indexes @p opts.
     *  @param opts Labels for selectable positions; must not be empty.
     *  @return The resulting selected index.
     */
    int SliderLabeled(const char* label, int* v, std::initializer_list<const char*> opts, const char* fmt = "%s");
}
