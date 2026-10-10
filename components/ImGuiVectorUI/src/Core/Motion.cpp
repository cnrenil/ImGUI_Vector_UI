#include "ImGuiVectorUI/Core/Motion.h"

#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Context.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace IVUI
{
    namespace
    {
        ImGuiID MotionStateId(ImGuiID id)
        {
            constexpr ImGuiID seed = 0x49564d4fu;
            return ImHashData(&id, sizeof(id), seed);
        }
    }

    void SetMotionSettings(const MotionSettings& settings)
    {
        if (Context* context = GetContext())
        {
            context->motion = settings;
            context->motion.speed = std::isfinite(settings.speed)
                ? std::clamp(settings.speed, 0.0f, 10.0f) : 1.0f;
        }
    }

    MotionSettings GetMotionSettings()
    {
        const Context* context = GetContext();
        return context ? context->motion : MotionSettings{};
    }

    float MotionResponse(float speed)
    {
        if (GetMotionSettings().reducedMotion) return 1.0f;
        if (!std::isfinite(speed) || speed <= 0.0f) return 0.0f;
        return -std::expm1(-DeltaTime() * speed);
    }

    float AdvanceProgress(float value, float target, float speed)
    {
        if (!std::isfinite(target)) return value;
        if (!std::isfinite(value) || GetMotionSettings().reducedMotion) return target;
        if (!std::isfinite(speed) || speed <= 0.0f) return value;
        const float step = DeltaTime() * speed;
        return value < target ? std::min(value + step, target)
                              : std::max(value - step, target);
    }

    ImU32 WithAlphaMul(ImU32 col, float a)
    {
        a = ImClamp(a, 0.0f, 1.0f);
        const int alpha = (int)(((col >> IM_COL32_A_SHIFT) & 0xFF) * a);
        return (col & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
    }

    float Approach(ImGuiID id, float target, float speed)
    {
        AnimSlot& a = SlotID(id);
        if (!a.initialized) { a.currentValue = target; a.initialized = true; }
        a.currentValue += (target - a.currentValue) * MotionResponse(speed);
        return a.currentValue;
    }
    float Approach(const char* label, float target, float speed)
    {
        return Approach(ImGui::GetID(label), target, speed);
    }

    float Toggle01(ImGuiID id, bool on, float speed, EasingType e)
    {
        AnimSlot& a = SlotID(id);
        a.direction = on ? 1.0f : -1.0f;
        a.t = AdvanceProgress(a.t, on ? 1.0f : 0.0f, speed);
        return EaseValue(0.0f, 1.0f, a.t, e);
    }
    float Toggle01(const char* label, bool on, float speed, EasingType e)
    {
        return Toggle01(ImGui::GetID(label), on, speed, e);
    }

    ImU32 ColorApproach(ImGuiID id, ImU32 target, float speed, EasingType e)
    {
        AnimSlot& a = SlotID(id);
        const ImVec4 t = ImGui::ColorConvertU32ToFloat4(target);
        if (!a.initialized) { a.colorVal = t; a.initialized = true; }
        // Preserve the chosen easing's 60 Hz response without changing it with FPS.
        const float reference = EaseValue(0.0f, 1.0f,
            std::isfinite(speed) ? ImClamp(speed / 60.0f, 0.0f, 1.0f) : 0.0f, e);
        const float k = GetMotionSettings().reducedMotion ? 1.0f
            : (DeltaTime() <= 0.0f ? 0.0f
                : 1.0f - std::pow(1.0f - ImClamp(reference, 0.0f, 1.0f), DeltaTime() * 60.0f));
        a.colorVal.x += (t.x - a.colorVal.x) * k;
        a.colorVal.y += (t.y - a.colorVal.y) * k;
        a.colorVal.z += (t.z - a.colorVal.z) * k;
        a.colorVal.w += (t.w - a.colorVal.w) * k;
        return ImGui::ColorConvertFloat4ToU32(a.colorVal);
    }
    ImU32 ColorApproach(const char* label, ImU32 target, float speed, EasingType e)
    {
        return ColorApproach(ImGui::GetID(label), target, speed, e);
    }

    float Transition(ImGuiID id, int key, float speed, EasingType e)
    {
        AnimSlot& a = SlotID(id);
        if (!a.initialized || a.transitionKey != key)
        {
            a.transitionKey = key;
            a.t = 0.0f;
            a.initialized = true;
        }
        a.t = AdvanceProgress(a.t, 1.0f, speed);
        return EaseValue(0.0f, 1.0f, a.t, e);
    }
    float Transition(const char* label, int key, float speed, EasingType e)
    {
        return Transition(ImGui::GetID(label), key, speed, e);
    }

    ItemMotionScope::ItemMotionScope(ItemMotionScope&& other) noexcept
        : drawList_(other.drawList_), vertexStart_(other.vertexStart_),
          offset_(other.offset_), scale_(other.scale_), alpha_(other.alpha_),
          progress_(other.progress_), frame_(other.frame_),
          contextLifetime_(std::move(other.contextLifetime_)), context_(other.context_),
          active_(other.active_), shouldDraw_(other.shouldDraw_)
    {
        other.drawList_ = nullptr;
        other.context_ = nullptr;
        other.active_ = false;
        other.shouldDraw_ = false;
    }

    ItemMotionScope& ItemMotionScope::operator=(ItemMotionScope&& other) noexcept
    {
        if (this != &other)
        {
            End();
            drawList_ = other.drawList_;
            vertexStart_ = other.vertexStart_;
            offset_ = other.offset_;
            scale_ = other.scale_;
            alpha_ = other.alpha_;
            progress_ = other.progress_;
            frame_ = other.frame_;
            contextLifetime_ = std::move(other.contextLifetime_);
            context_ = other.context_;
            active_ = other.active_;
            shouldDraw_ = other.shouldDraw_;
            other.drawList_ = nullptr;
            other.context_ = nullptr;
            other.active_ = false;
            other.shouldDraw_ = false;
        }
        return *this;
    }

    ItemMotionScope::~ItemMotionScope()
    {
        End();
    }

    void ItemMotionScope::End() noexcept
    {
        if (!active_)
            return;
        Context* context = contextLifetime_.expired()
            ? nullptr : static_cast<Context*>(context_);
        if (context && GetContext() == context && context->frameActive &&
            context->frameCount == frame_ && drawList_ && shouldDraw_)
        {
            const int vertexEnd = drawList_->VtxBuffer.Size;
            if (vertexEnd > vertexStart_)
            {
                ImVec2 boundsMin(FLT_MAX, FLT_MAX);
                ImVec2 boundsMax(-FLT_MAX, -FLT_MAX);
                for (int index = vertexStart_; index < vertexEnd; ++index)
                {
                    const ImVec2 position = drawList_->VtxBuffer[index].pos;
                    boundsMin.x = std::min(boundsMin.x, position.x);
                    boundsMin.y = std::min(boundsMin.y, position.y);
                    boundsMax.x = std::max(boundsMax.x, position.x);
                    boundsMax.y = std::max(boundsMax.y, position.y);
                }
                const ImVec2 pivot((boundsMin.x + boundsMax.x) * 0.5f,
                                   (boundsMin.y + boundsMax.y) * 0.5f);
                const float alpha = ImClamp(alpha_, 0.0f, 1.0f);
                for (int index = vertexStart_; index < vertexEnd; ++index)
                {
                    ImDrawVert& vertex = drawList_->VtxBuffer[index];
                    vertex.pos.x = pivot.x + (vertex.pos.x - pivot.x) * scale_ + offset_.x;
                    vertex.pos.y = pivot.y + (vertex.pos.y - pivot.y) * scale_ + offset_.y;
                    const unsigned sourceAlpha =
                        (vertex.col >> IM_COL32_A_SHIFT) & 0xffu;
                    vertex.col = (vertex.col & ~IM_COL32_A_MASK) |
                        (static_cast<ImU32>(sourceAlpha * alpha) << IM_COL32_A_SHIFT);
                }
            }
        }
        drawList_ = nullptr;
        contextLifetime_.reset();
        context_ = nullptr;
        active_ = false;
        shouldDraw_ = false;
    }

    ItemMotionScope BeginItemMotion(ImDrawList* drawList, ImGuiID id, bool visible,
                                    const ItemMotionStyle& style)
    {
        ItemMotionScope scope;
        Context* context = GetContext();
        if (!drawList || !context || !context->frameActive || id == 0)
            return scope;

        AnimSlot& state = context->anim.Get(MotionStateId(id), context->frameCount);
        if (!state.initialized)
        {
            state.t = visible && !style.animateInitial ? 1.0f : 0.0f;
            state.initialized = true;
        }
        const float duration = visible ? style.enterDuration : style.exitDuration;
        if (!std::isfinite(duration) || duration <= 0.0001f)
            state.t = visible ? 1.0f : 0.0f;
        else
            state.t = AdvanceProgress(state.t, visible ? 1.0f : 0.0f, 1.0f / duration);

        const float eased = visible
            ? EaseValue(0.0f, 1.0f, state.t, style.enterEasing)
            : 1.0f - EaseValue(0.0f, 1.0f, 1.0f - state.t, style.exitEasing);
        scope.drawList_ = drawList;
        scope.vertexStart_ = drawList->VtxBuffer.Size;
        scope.offset_ = ImVec2(style.offset.x * (1.0f - eased),
                               style.offset.y * (1.0f - eased));
        scope.scale_ = style.scale + (1.0f - style.scale) * eased;
        scope.alpha_ = style.minimumAlpha + (1.0f - style.minimumAlpha) * eased;
        scope.progress_ = eased;
        scope.frame_ = context->frameCount;
        scope.contextLifetime_ = context->lifetime;
        scope.context_ = context;
        scope.active_ = true;
        scope.shouldDraw_ = visible || state.t > 0.0001f;
        return scope;
    }

    ItemMotionScope BeginItemMotion(ImGuiID id, bool visible,
                                    const ItemMotionStyle& style)
    {
        return BeginItemMotion(ImGui::GetWindowDrawList(), id, visible, style);
    }

    ItemMotionScope BeginItemMotion(const char* label, bool visible,
                                    const ItemMotionStyle& style)
    {
        return BeginItemMotion(ImGui::GetID(label), visible, style);
    }
}
