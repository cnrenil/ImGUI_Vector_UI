/** @file Element.h @brief Theme-resolved visual scope for arbitrary ImGui items. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Theme/Theme.h"

namespace IVUI
{
    struct ElementOptions
    {
        ElementRole role = ElementRole::Default;
        /** Null inherits the selected theme role; a pointer to an empty request disables effects. */
        const ItemEffectRequest* effects = nullptr;
        /** Null inherits the selected theme role. */
        const ItemMotionStyle* motion = nullptr;
    };

    /** Captures both effects and motion in the required destruction order. */
    class ElementScope
    {
    public:
        ElementScope() noexcept = default;
        ElementScope(ElementScope&&) noexcept = default;
        ElementScope& operator=(ElementScope&& other) noexcept;
        ElementScope(const ElementScope&) = delete;
        ElementScope& operator=(const ElementScope&) = delete;

        explicit operator bool() const noexcept { return static_cast<bool>(motion_); }
        bool HasEffect() const noexcept { return static_cast<bool>(effect_); }
        float Progress() const noexcept { return motion_.Progress(); }
        void End() noexcept;

    private:
        friend ElementScope BeginElement(ImDrawList*, ImGuiID, bool, const ElementOptions&);
        ItemEffectScope effect_;
        ItemMotionScope motion_;
    };

    /** Begins a theme-resolved scope around geometry emitted after this call. */
    ElementScope BeginElement(ImDrawList* drawList, ImGuiID id, bool visible,
                              const ElementOptions& options = {});
    ElementScope BeginElement(ImGuiID id, bool visible, const ElementOptions& options = {});
    ElementScope BeginElement(const char* label, bool visible, const ElementOptions& options = {});
}
