/** @file Basic.h @brief Themed wrappers for common ImGui widgets. */
#pragma once

#include "imgui.h"

#include <cfloat>
#include <climits>
#include <cstddef>

namespace IVUI
{
    /** @return True when the value changed. A null value is drawn disabled. */
    bool Checkbox(const char* label, bool* value);

    /** @return True when the radio button was activated. */
    bool RadioButton(const char* label, bool active);
    /** @return True when @p value changed to @p buttonValue. A null value is drawn disabled. */
    bool RadioButton(const char* label, int* value, int buttonValue);

    /** Draws an animated progress bar. Non-finite fractions are treated as zero. */
    void ProgressBar(const char* label, float fraction,
                     ImVec2 size = ImVec2(0.0f, 0.0f), const char* overlay = nullptr);

    /** Draws a non-interactive indeterminate activity indicator. */
    void Spinner(const char* label, float radius = 0.0f, float thickness = 0.0f);

    /** Draws a bounded integer input. Reversed bounds are accepted and normalized. */
    bool InputInt(const char* label, int* value,
                  int minValue = INT_MIN, int maxValue = INT_MAX,
                  int step = 1, int stepFast = 100,
                  ImGuiInputTextFlags flags = 0);

    /** Draws a bounded floating-point input. Reversed bounds are accepted and normalized. */
    bool InputFloat(const char* label, float* value,
                    float minValue = -FLT_MAX, float maxValue = FLT_MAX,
                    float step = 0.0f, float stepFast = 0.0f,
                    const char* format = "%.3f", ImGuiInputTextFlags flags = 0);

    /** Draws a multiline text input. A null or zero-capacity buffer is drawn disabled. */
    bool InputTextMultiline(const char* label, char* buffer, std::size_t bufferSize,
                            ImVec2 size = ImVec2(0.0f, 0.0f),
                            ImGuiInputTextFlags flags = 0,
                            ImGuiInputTextCallback callback = nullptr, void* userData = nullptr);

    /** Draws a full-width search field with a magnifier and optional placeholder. */
    bool SearchInput(const char* label, char* buffer, std::size_t bufferSize,
                     const char* hint = "Search", float width = 0.0f,
                     ImGuiInputTextFlags flags = 0,
                     ImGuiInputTextCallback callback = nullptr, void* userData = nullptr);

    /** Native ImGui selectable behavior with IVUI theme colors. */
    bool Selectable(const char* label, bool selected = false,
                    ImGuiSelectableFlags flags = 0, ImVec2 size = ImVec2(0.0f, 0.0f));
    /** Pointer overload that toggles @p selected. A null value is drawn disabled. */
    bool Selectable(const char* label, bool* selected,
                    ImGuiSelectableFlags flags = 0, ImVec2 size = ImVec2(0.0f, 0.0f));
}
