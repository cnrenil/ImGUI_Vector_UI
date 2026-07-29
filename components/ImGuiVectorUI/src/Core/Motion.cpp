#include "ImGuiVectorUI/Core/Motion.h"

#include "ImGuiVectorUI/Core/Id.h"

#include "imgui_internal.h"

namespace IVUI
{
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
        a.currentValue += (target - a.currentValue) * ImClamp(DeltaTime() * speed, 0.0f, 1.0f);
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
        a.t = ImClamp(a.t + DeltaTime() * speed * a.direction, 0.0f, 1.0f);
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
        const float k = EaseValue(0.0f, 1.0f, ImClamp(DeltaTime() * speed, 0.0f, 1.0f), e);
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
        if (a.lastTarget != (float)key) { a.lastTarget = (float)key; a.t = 0.0f; }
        a.t = ImClamp(a.t + DeltaTime() * speed, 0.0f, 1.0f);
        return EaseValue(0.0f, 1.0f, a.t, e);
    }
    float Transition(const char* label, int key, float speed, EasingType e)
    {
        return Transition(ImGui::GetID(label), key, speed, e);
    }
}
