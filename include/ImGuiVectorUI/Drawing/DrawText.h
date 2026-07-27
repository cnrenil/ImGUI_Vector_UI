/** @file DrawText.h @brief Theme-scaled text measurement and drawing. */
#pragma once

#include "imgui.h"

namespace IVUI
{
    /** @brief Returns the regular font selected by the theme or context. */
    ImFont* GetRegularFont();
    ImFont* GetTitleFont();

    /** @brief Returns the end of the visible portion of a label. */
    const char* VisibleEnd(const char* label);

    /** @brief Measures a label at the theme-scaled point size @p pt.
     *  @param label Text whose ID suffix is excluded from measurement.
     *  @param pt Unscaled theme point size.
     *  @param font Optional font override.
     *  @return Text size in screen pixels.
     */
    ImVec2 CalcTextScaled(const char* label, float pt, ImFont* font = nullptr);
    /** @brief Adds theme-scaled text to a draw list.
     *  @param draw Target draw list.
     *  @param pos Screen position in pixels.
     *  @param col Packed RGBA text color.
     *  @param label Text whose ID suffix is not rendered.
     *  @param pt Unscaled theme point size.
     *  @param font Optional font override.
     */
    void   AddTextScaled(ImDrawList* draw, ImVec2 pos, ImU32 col, const char* label, float pt, ImFont* font = nullptr);
}
