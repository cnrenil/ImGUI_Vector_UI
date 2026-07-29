/** @file Combo.h @brief Animated single- and multi-select dropdown widgets. */
#pragma once

#include <initializer_list>
#include "ImGuiVectorUI/Core/Types.h"

namespace IVUI
{
    /** @brief Draws a single-select dropdown indexed by @p index.
     *  @param items Non-empty list of visible item labels.
     *  @return True when @p index changed.
     */
    bool Combo(const char* label, int* index, std::initializer_list<const char*> items);

    /** @brief Draws a multi-select dropdown whose items own their selected flags.
     *  @param items Labels and mutable selection flags.
     *  @return True when any selected flag changed.
     */
    bool ComboMulti(const char* label, std::initializer_list<ComboItem> items);
}
