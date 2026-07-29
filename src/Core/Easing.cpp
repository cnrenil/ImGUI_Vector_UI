#include "ImGuiVectorUI/Core/Easing.h"

#include <algorithm>
#include <cmath>

namespace IVUI
{
    float EaseValue(float start, float end, float t, EasingType type)
    {
        constexpr float pi = 3.14159265358979323846f;
        t = std::clamp(t, 0.0f, 1.0f);
        float e = 0.0f;
        switch (type)
        {
        case EASE_LINEAR:       e = t; break;
        case EASE_IN_QUAD:      e = t * t; break;
        case EASE_OUT_QUAD:     e = 1.0f - (1.0f - t) * (1.0f - t); break;
        case EASE_IN_OUT_QUAD:
            e = (t < 0.5f) ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
            break;
        case EASE_IN_CUBIC:     e = t * t * t; break;
        case EASE_OUT_CUBIC:    e = 1.0f - powf(1.0f - t, 3.0f); break;
        case EASE_IN_OUT_CUBIC:
            e = (t < 0.5f) ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
            break;
        case EASE_IN_QUART:     e = t * t * t * t; break;
        case EASE_OUT_QUART:    e = 1.0f - powf(1.0f - t, 4.0f); break;
        case EASE_IN_OUT_QUART:
            e = (t < 0.5f) ? 8.0f * t * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 4.0f) / 2.0f;
            break;
        case EASE_IN_SINE:      e = 1.0f - cosf((t * pi) / 2.0f); break;
        case EASE_OUT_SINE:     e = sinf((t * pi) / 2.0f); break;
        case EASE_IN_OUT_SINE:  e = -(cosf(pi * t) - 1.0f) / 2.0f; break;
        case EASE_IN_EXPO:      e = (t == 0.0f) ? 0.0f : powf(2.0f, 10.0f * t - 10.0f); break;
        case EASE_OUT_EXPO:     e = (t == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); break;
        case EASE_IN_OUT_EXPO:
            if (t == 0.0f)      e = 0.0f;
            else if (t == 1.0f) e = 1.0f;
            else if (t < 0.5f)  e = powf(2.0f, 20.0f * t - 10.0f) / 2.0f;
            else                e = (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
            break;
        case EASE_IN_CIRC:      e = 1.0f - sqrtf(1.0f - powf(t, 2.0f)); break;
        case EASE_OUT_CIRC:     e = sqrtf(1.0f - powf(t - 1.0f, 2.0f)); break;
        case EASE_IN_OUT_CIRC:
            e = (t < 0.5f)
                ? (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f
                : (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
            break;
        case EASE_OUT_ELASTIC:
        {
            const float c4 = (2.0f * pi) / 3.0f;
            const float amplitude = 0.2f; // subtle bounce
            if (t == 0.0f)      e = 0.0f;
            else if (t == 1.0f) e = 1.0f;
            else                e = powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) * amplitude + 1.0f;
            break;
        }
        default: e = t; break;
        }
        return start + (end - start) * e;
    }

    float EaseValueCalc(float start, float end, float t, EasingType type)
    {
        return EaseValue(start, end, t, type);
    }

    bool EaseValueColor(ImColor start, ImColor end, ImColor* v, float t, EasingType type)
    {
        if (v == nullptr)
            return false;
        v->Value.x = EaseValue(start.Value.x, end.Value.x, t, type);
        v->Value.y = EaseValue(start.Value.y, end.Value.y, t, type);
        v->Value.z = EaseValue(start.Value.z, end.Value.z, t, type);
        v->Value.w = EaseValue(start.Value.w, end.Value.w, t, type);
        return t >= 1.0f;
    }

    ImColor EaseValueColor(ImColor start, ImColor end, float t, EasingType type)
    {
        ImColor v;
        v.Value.x = EaseValue(start.Value.x, end.Value.x, t, type);
        v.Value.y = EaseValue(start.Value.y, end.Value.y, t, type);
        v.Value.z = EaseValue(start.Value.z, end.Value.z, t, type);
        v.Value.w = EaseValue(start.Value.w, end.Value.w, t, type);
        return v;
    }

    bool EaseValueImVec2(ImVec2 start, ImVec2 end, ImVec2* v, float t, EasingType type)
    {
        if (v == nullptr)
            return false;
        v->x = EaseValue(start.x, end.x, t, type);
        v->y = EaseValue(start.y, end.y, t, type);
        return t >= 1.0f;
    }

    ImVec2 EaseValueImVec2(ImVec2 start, ImVec2 end, float t, EasingType type)
    {
        return ImVec2(EaseValue(start.x, end.x, t, type), EaseValue(start.y, end.y, t, type));
    }
}
