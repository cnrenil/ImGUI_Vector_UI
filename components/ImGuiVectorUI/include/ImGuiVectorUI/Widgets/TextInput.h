/** @file TextInput.h @brief Themed single-line text input widget. */
#pragma once

#include <cstddef>

namespace IVUI
{
    /** @brief Draws a full-width input.
     *  @param label Visible label and ImGui ID source.
     *  @param buf Mutable, null-terminated text buffer.
     *  @param bufSize Capacity of @p buf in bytes.
     *  @param hint Optional placeholder text.
     *  @return True when @p buf changed.
     */
    bool TextInput(const char* label, char* buf, size_t bufSize, const char* hint = nullptr);
}
