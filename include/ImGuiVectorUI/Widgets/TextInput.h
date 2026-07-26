// ImGuiVectorUI — themed single-line text input.
#pragma once

#include <cstddef>

namespace IVUI
{
    // Full-width themed input. Returns true when the text changed.
    bool TextInput(const char* label, char* buf, size_t bufSize, const char* hint = nullptr);
}
