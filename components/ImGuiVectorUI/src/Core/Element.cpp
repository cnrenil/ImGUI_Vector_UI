#include "ImGuiVectorUI/Core/Element.h"

#include "ImGuiVectorUI/Core/Context.h"

#include <utility>

namespace IVUI
{
    ElementScope& ElementScope::operator=(ElementScope&& other) noexcept
    {
        if (this != &other)
        {
            End();
            effect_ = std::move(other.effect_);
            motion_ = std::move(other.motion_);
        }
        return *this;
    }

    void ElementScope::End() noexcept
    {
        motion_.End();
        effect_.End();
    }

    ElementScope BeginElement(ImDrawList* drawList, ImGuiID id, bool visible,
                              const ElementOptions& options)
    {
        ElementScope scope;
        const ElementVisualStyle visual = GetTheme().ResolveElement(options.role);
        const ItemEffectRequest& effects = options.effects ? *options.effects : visual.effects;
        const ItemMotionStyle& motion = options.motion ? *options.motion : visual.motion;

        if (effects.layerCount != 0)
            if (Context* context = GetContext(); context && context->effectRenderer)
                scope.effect_ = BeginItemEffect(drawList, *context->effectRenderer, effects);
        scope.motion_ = BeginItemMotion(drawList, id, visible, motion);
        return scope;
    }

    ElementScope BeginElement(ImGuiID id, bool visible, const ElementOptions& options)
    {
        return BeginElement(ImGui::GetWindowDrawList(), id, visible, options);
    }

    ElementScope BeginElement(const char* label, bool visible, const ElementOptions& options)
    {
        return BeginElement(ImGui::GetWindowDrawList(), ImGui::GetID(label), visible, options);
    }
}
