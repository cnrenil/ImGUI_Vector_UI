/** @file Effects.h
 *  @brief Renderer-independent draw-list bridge for glass backdrops.
 */
#pragma once

#include "imgui.h"

#include <cstddef>
#include <type_traits>

namespace IVUI
{
    /** @brief Screen-space parameters for one glass backdrop. */
    struct GlassRect
    {
        ImVec2 min, max;      ///<  rect in screen pixels
        float  rounding;      ///<  corner radius in pixels
        float  opacity;       ///<  0..1 fill strength of the glass tint
        ImU32  tint;          ///<  glass body color (alpha ignored; opacity drives it)
        float  tintStrength;  ///<  0 = pure backdrop blur, 1 = fully tinted glass
        ImU32  accent;        ///<  edge-sheen / accent color
        float  refraction;    ///<  px of backdrop displacement at the material rim
        float  edgeWidth;     ///<  px width of the refractive rim
        float  edgeHighlight; ///<  0..1 directional specular rim strength
        float  thickness;     ///<  0..1 optical thickness of the glass body
        float  glowStrength;  ///<  0..1 outer glow intensity
        float  glowSpread;    ///<  px falloff distance of the outer glow
        float  shadowStrength;///<  0..1 drop-shadow intensity
        float  shadowSpread;  ///<  px falloff distance of the drop shadow
    };

    /** @brief Host hook invoked from an ImGui draw-list callback at a panel's Z position. */
    using GlassRenderCallback = void (*)(const GlassRect& rect);

    /** @brief Binary constant layout consumed by EffectsShaderSource().
     *
     *  Each entry maps directly to one HLSL @c float4 in @c uData. Hosts must
     *  upload all 32 floats as 32-bit root constants or an equivalent uniform
     *  buffer layout.
     */
    struct GlassShaderConstants
    {
        float values[32]{};
    };
    static_assert(std::is_standard_layout_v<GlassShaderConstants> &&
                  std::is_trivially_copyable_v<GlassShaderConstants> &&
                  sizeof(GlassShaderConstants) == sizeof(float) * 32,
                  "Glass shader constants must match HLSL float4 uData[8].");
    /** @brief Installs the host compositor used to render glass at ImGui window depth. */
    void              SetGlassRenderCallback(GlassRenderCallback callback);
    /** @brief Inserts a backdrop compositor callback into the current ImGui window.
     *  @param r Screen-space backdrop parameters.
     */
    void SubmitGlass(const GlassRect& r);

    /** @brief Builds constants for one separable Gaussian blur pass.
     *  @param renderTargetSize Source texture dimensions in pixels.
     *  @param direction Blur axis, normally (1, 0) or (0, 1).
     *  @param sigma Gaussian standard deviation in pixels.
     *  @return Constants compatible with the @c GaussianPS entry point.
     */
    GlassShaderConstants MakeGaussianBlurConstants(ImVec2 renderTargetSize, ImVec2 direction, float sigma);
    /** @brief Builds constants for the opaque host background pass.
     *  @param renderTargetSize Background dimensions in pixels.
     *  @param timeSeconds Monotonic animation time in seconds.
     *  @return Constants compatible with the @c BackgroundPS entry point.
     */
    GlassShaderConstants MakeBackgroundConstants(ImVec2 renderTargetSize, float timeSeconds);
    /** @brief Builds constants for filtering ImGui pixels before blur passes.
     *  @param colorMax Inclusive per-component color ceiling.
     *  @param colorMin Inclusive per-component color floor.
     *  @param averageThreshold Minimum average RGB brightness.
     *  @param saturationThreshold Minimum RGB chroma; neutral text is excluded.
     *  @return Constants compatible with the @c FilterPS entry point.
     */
    GlassShaderConstants MakeBloomFilterConstants(ImVec4 colorMax, ImVec4 colorMin, float averageThreshold, float saturationThreshold);
    /** @brief Builds constants for additive bloom compositing of a blurred UI frame.
     *  @param strength Additive bloom intensity.
     *  @return Constants compatible with the @c GlowPS entry point.
     */
    GlassShaderConstants MakeGlowCompositeConstants(float strength);
    /** @brief Builds constants for one glass-composite draw.
     *  @param rect Screen-space glass request to encode.
     *  @param renderTargetSize Backdrop texture dimensions in pixels.
     *  @return Constants compatible with the @c CompositePS entry point.
     */
    GlassShaderConstants MakeGlassCompositeConstants(const GlassRect& rect, ImVec2 renderTargetSize);
    /** @brief Returns reusable HLSL for background, Gaussian blur, glass and glow effects.
     *  @return UTF-8 HLSL source containing the effects pass entry points.
     */
    const char* EffectsShaderSource();

}
