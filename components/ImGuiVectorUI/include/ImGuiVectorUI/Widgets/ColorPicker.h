/** @file ColorPicker.h @brief Themed color picker widget. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Element.h"

#include <array>
#include <cstddef>
#include <vector>

namespace IVUI
{
    inline constexpr std::size_t ColorPickerHistoryCapacity = 12;
    inline constexpr std::size_t ColorPickerFavoriteCapacity = 12;

    /** @brief Host-provided screen color sampler used by the eyedropper button.
     *  @param screenPosition ImGui screen-space position at the time of activation.
     *  @param outputColor Receives the sampled color when the callback succeeds.
     *  @param userData Opaque host-owned callback data.
     *  @return True when @p outputColor contains a valid sampled color.
     */
    using ColorPickerEyedropperCallback = bool (*)(ImVec2 screenPosition,
                                                   ImVec4& outputColor,
                                                   void* userData);

    struct ColorPickerOptions
    {
        bool showAlpha = true;
        bool showHistory = true;
        bool showFavorites = true;
        bool showRelated = true;
        bool showEyedropper = true;
        const ElementOptions* element = nullptr;
    };

    /** @brief Palette and host integration state owned by the active IVUI context. */
    struct ColorPickerState
    {
        std::vector<ImVec4> history;
        std::vector<ImVec4> favorites;
        ColorPickerEyedropperCallback eyedropper = nullptr;
        void* eyedropperUserData = nullptr;

        // Immediate-mode editing state is kept per UI instance, never globally.
        std::array<char, 9> hexBuffer{};
        ImGuiID hexBufferId = 0;
        ImGuiID pendingHistoryId = 0;
        ImVec4 pendingHistoryColor{};
        bool historyPending = false;
    };

    /** @return Mutable color-picker state for the active context, or null. */
    ColorPickerState* GetColorPickerState() noexcept;

    /** @brief Registers the host screen sampler for the active context. */
    void SetColorPickerEyedropper(ColorPickerEyedropperCallback callback,
                                  void* userData = nullptr) noexcept;

    /** @brief Adds a color to the active context's favorite palette. */
    bool AddColorPickerFavorite(const ImVec4& color);
    /** @brief Removes a matching color from the active context's favorite palette. */
    bool RemoveColorPickerFavorite(const ImVec4& color);
    /** @brief Clears the active context's recent-color history. */
    void ClearColorPickerHistory() noexcept;
    /** @brief Clears the active context's favorite palette. */
    void ClearColorPickerFavorites() noexcept;

    /** @brief Draws a labeled color swatch and popup editor.
     *  @return True when @p col changed.
     */
    bool ColorPicker(const char* label, ImVec4* col);
    /** @brief Draws the themed picker with feature visibility options.
     *  @return True when @p col changed.
     */
    bool ColorPicker(const char* label, ImVec4* col,
                     const ColorPickerOptions& options);
}
