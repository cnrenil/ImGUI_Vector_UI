#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Core/Context.h"

#include <stdexcept>

namespace IVUI
{
    static GlassRenderCallback g_glassRenderCallback = nullptr;

    static void RenderGlassDrawCallback(const ImDrawList*, const ImDrawCmd* cmd)
    {
        if (g_glassRenderCallback != nullptr && cmd->UserCallbackData != nullptr)
            g_glassRenderCallback(*static_cast<const GlassRect*>(cmd->UserCallbackData));
    }

    const char* EffectsShaderSource()
    {
        return R"(
Texture2D SourceTex : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer Constants : register(b0) { float4 uData[6]; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o; float2 p = float2((vid == 2) ? 3.0 : -1.0, (vid == 1) ? 3.0 : -1.0);
    o.pos = float4(p, 0.0, 1.0); o.uv = float2((p.x + 1.0) * 0.5, 1.0 - (p.y + 1.0) * 0.5); return o;
}
float4 GaussianPS(VSOut input) : SV_Target {
    float2 texel = uData[0].xy, direction = uData[0].zw; float sigma = max(uData[1].x, 0.5);
    float4 sum = 0.0; float weightSum = 0.0;
    [unroll] for (int sampleIndex = -12; sampleIndex <= 12; ++sampleIndex) {
        float distance = (float)sampleIndex, weight = exp(-0.5 * distance * distance / (sigma * sigma));
        sum += SourceTex.Sample(LinearClamp, input.uv + direction * texel * distance) * weight; weightSum += weight;
    } return sum / weightSum;
}
float4 BackdropGaussianPS(VSOut input) : SV_Target {
    float2 texel = uData[0].xy, direction = uData[0].zw; float sigma = max(uData[1].x, 0.5);
    float4 sum = 0.0; float weightSum = 0.0;
    [unroll] for (int sampleIndex = -32; sampleIndex <= 32; ++sampleIndex) {
        float distance = (float)sampleIndex, weight = exp(-0.5 * distance * distance / (sigma * sigma));
        sum += SourceTex.Sample(LinearClamp, input.uv + direction * texel * distance) * weight; weightSum += weight;
    } return sum / weightSum;
}
float4 BackgroundPS(VSOut input) : SV_Target {
    float2 resolution = max(uData[0].xy, float2(1.0, 1.0)), uv = input.uv;
    float2 p = (uv - 0.5) * float2(resolution.x / resolution.y, 1.0);
    float time = uData[0].z;
    float2 cell = floor(uv * float2(30.0, 18.0) + float2(time * 0.12, 0.0));
    float hue = frac(dot(cell, float2(0.071, 0.173)));
    float3 rainbow = 0.5 + 0.5 * cos(6.2831853 * (hue + float3(0.0, 0.3333333, 0.6666667)));
    float vignette = 1.0 - saturate(dot(p, p) * 0.46);
    float3 color = float3(0.006, 0.008, 0.014);
    color += rainbow * (0.26 + 0.20 * vignette);
    return float4(color, 1.0);
}
float4 FilterPS(VSOut input) : SV_Target {
    float4 color = SourceTex.Sample(LinearClamp, input.uv);
    float4 colorMin = uData[0], colorMax = uData[1];
    bool componentMatch = all(color >= colorMin) && all(color <= colorMax);
    float average = (color.r + color.g + color.b) / 3.0;
    float chroma = max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
    float brightnessThreshold = saturate(uData[2].x);
    float saturationThreshold = saturate(uData[2].y);
    float brightnessWeight = smoothstep(brightnessThreshold - 0.25, brightnessThreshold + 0.05, average);
    float saturationWeight = smoothstep(saturationThreshold - 0.05, saturationThreshold + 0.05, chroma);
    float filterWeight = (componentMatch ? 1.0 : 0.0) * brightnessWeight * saturationWeight;
    return color * filterWeight;
}
float RoundedBoxSdf(float2 p, float2 center, float2 halfSize, float radius) {
    float2 q = abs(p - center) - halfSize + radius; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}
float4 Over(float4 accumulated, float3 color, float alpha) {
    alpha = saturate(alpha); accumulated.rgb = color * alpha + accumulated.rgb * (1.0 - alpha);
    accumulated.a = alpha + accumulated.a * (1.0 - alpha); return accumulated;
}
float4 CompositePS(VSOut input) : SV_Target {
    float2 rectMin = uData[0].xy, rectMax = uData[0].zw, center = (rectMin + rectMax) * 0.5, halfSize = (rectMax - rectMin) * 0.5;
    float radius = uData[1].z, opacity = uData[1].w; float3 tint = uData[2].rgb, accent = uData[3].rgb; float4 fx = uData[5];
    float sdf = RoundedBoxSdf(input.pos.xy, center, halfSize, radius), mask = saturate(1.0 - smoothstep(0.0, 1.5, sdf));
    float border = saturate(1.0 - smoothstep(1.0, 3.5, abs(sdf)));
    float shadowSdf = RoundedBoxSdf(input.pos.xy, center + float2(0.0, 10.0), halfSize, radius);
    float shadowAlpha = exp(-max(shadowSdf, 0.0) / max(fx.w, 0.001)) * fx.z * (1.0 - mask);
    float tintStrength = saturate(uData[4].w);
    float3 glass = lerp(SourceTex.Sample(LinearClamp, input.uv).rgb, tint, tintStrength);
    glass += accent * (0.08 + border * 0.28);
    float4 result = float4(0.0, 0.0, 0.0, 0.0); result = Over(result, float3(0.0, 0.0, 0.0), shadowAlpha);
    return Over(result, glass, mask * opacity);
}
float4 GlowPS(VSOut input) : SV_Target {
    float3 blurred = SourceTex.Sample(LinearClamp, input.uv).rgb;
    float3 outColorBlend = blurred;
    float bloom = uData[0].x;
    return float4(outColorBlend * bloom, 0.0);
}
 )";
    }

    void SetGlassRenderCallback(GlassRenderCallback callback) { g_glassRenderCallback = callback; }

    GlassShaderConstants MakeGaussianBlurConstants(ImVec2 renderTargetSize, ImVec2 direction, float sigma)
    {
        GlassShaderConstants constants{};
        if (renderTargetSize.x > 0.0f)
            constants.values[0] = 1.0f / renderTargetSize.x;
        if (renderTargetSize.y > 0.0f)
            constants.values[1] = 1.0f / renderTargetSize.y;
        constants.values[2] = direction.x;
        constants.values[3] = direction.y;
        constants.values[4] = sigma;
        return constants;
    }

    GlassShaderConstants MakeBackgroundConstants(ImVec2 renderTargetSize, float timeSeconds)
    {
        GlassShaderConstants constants{};
        constants.values[0] = renderTargetSize.x;
        constants.values[1] = renderTargetSize.y;
        constants.values[2] = timeSeconds;
        return constants;
    }

    GlassShaderConstants MakeBloomFilterConstants(ImVec4 colorMax, ImVec4 colorMin, float averageThreshold, float saturationThreshold)
    {
        GlassShaderConstants constants{};
        constants.values[0] = colorMin.x;
        constants.values[1] = colorMin.y;
        constants.values[2] = colorMin.z;
        constants.values[3] = colorMin.w;
        constants.values[4] = colorMax.x;
        constants.values[5] = colorMax.y;
        constants.values[6] = colorMax.z;
        constants.values[7] = colorMax.w;
        constants.values[8] = averageThreshold;
        constants.values[9] = saturationThreshold;
        return constants;
    }

    GlassShaderConstants MakeGlowCompositeConstants(float strength)
    {
        GlassShaderConstants constants{};
        constants.values[0] = strength;
        return constants;
    }

    GlassShaderConstants MakeGlassCompositeConstants(const GlassRect& rect)
    {
        GlassShaderConstants constants{};
        constants.values[0] = rect.min.x;
        constants.values[1] = rect.min.y;
        constants.values[2] = rect.max.x;
        constants.values[3] = rect.max.y;
        constants.values[6] = rect.rounding;
        constants.values[7] = rect.opacity;
        constants.values[8] = ((rect.tint >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        constants.values[9] = ((rect.tint >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        constants.values[10] = ((rect.tint >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
        constants.values[12] = ((rect.accent >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        constants.values[13] = ((rect.accent >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        constants.values[14] = ((rect.accent >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
        constants.values[16] = ((rect.glow >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        constants.values[17] = ((rect.glow >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        constants.values[18] = ((rect.glow >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
        constants.values[19] = rect.tintStrength;
        constants.values[20] = rect.glowStrength;
        constants.values[21] = rect.glowSpread;
        constants.values[22] = rect.shadowStrength;
        constants.values[23] = rect.shadowSpread;
        return constants;
    }

    void SubmitGlass(const GlassRect& r)
    {
        Context* ctx = GetContext();
        if (ctx == nullptr)
            throw std::logic_error("IVUI::SubmitGlass requires an active IVUI context.");

        // The callback runs where this window appears in ImGui's final draw order,
        // so its backdrop samples only panels that are actually behind it.
        if (g_glassRenderCallback == nullptr)
            return;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddCallback(RenderGlassDrawCallback, (void*)&r, sizeof(r));
        draw->AddCallback(ImDrawCallback_ResetRenderState);
    }
}
