/** @file Effects.h
 *  @brief Renderer-independent effect protocol and ImGui draw-list bridge.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

struct ImDrawList;
struct ImDrawData;

namespace IVUI
{
    class IEffectRenderer;

    enum class EffectId : std::uint32_t
    {
        BackdropBlur = 1,
        Glass = 2,
        Shadow = 3,
        Glow = 4,
        Bloom = 5,
        CustomFirst = 0x00010000u
    };

    enum class EffectCapabilities : std::uint32_t
    {
        None = 0,
        BackdropCapture = 1u << 0,
        BackdropBlur = 1u << 1,
        GlassMaterial = 1u << 2,
        Shadow = 1u << 3,
        Glow = 1u << 4,
        Bloom = 1u << 5,
        ItemEffects = 1u << 6,
        ItemSourceReplacement = 1u << 7
    };

    constexpr EffectCapabilities operator|(EffectCapabilities lhs, EffectCapabilities rhs) noexcept
    {
        return static_cast<EffectCapabilities>(static_cast<std::uint32_t>(lhs) |
                                               static_cast<std::uint32_t>(rhs));
    }

    constexpr EffectCapabilities operator&(EffectCapabilities lhs, EffectCapabilities rhs) noexcept
    {
        return static_cast<EffectCapabilities>(static_cast<std::uint32_t>(lhs) &
                                               static_cast<std::uint32_t>(rhs));
    }

    constexpr EffectCapabilities operator~(EffectCapabilities value) noexcept
    {
        return static_cast<EffectCapabilities>(~static_cast<std::uint32_t>(value));
    }

    constexpr EffectCapabilities& operator|=(EffectCapabilities& lhs, EffectCapabilities rhs) noexcept
    {
        lhs = lhs | rhs;
        return lhs;
    }

    inline constexpr EffectCapabilities AllEffectCapabilities =
        EffectCapabilities::BackdropCapture | EffectCapabilities::BackdropBlur |
        EffectCapabilities::GlassMaterial | EffectCapabilities::Shadow |
        EffectCapabilities::Glow | EffectCapabilities::Bloom |
        EffectCapabilities::ItemEffects | EffectCapabilities::ItemSourceReplacement;

    constexpr bool HasEffectCapabilities(EffectCapabilities available,
                                         EffectCapabilities required) noexcept
    {
        return (available & required) == required;
    }

    constexpr EffectCapabilities RequiredEffectCapabilities(EffectId id) noexcept
    {
        switch (id)
        {
        case EffectId::BackdropBlur:
            return EffectCapabilities::BackdropCapture | EffectCapabilities::BackdropBlur;
        case EffectId::Glass:
            return EffectCapabilities::BackdropCapture | EffectCapabilities::BackdropBlur |
                   EffectCapabilities::GlassMaterial;
        case EffectId::Shadow:
            return EffectCapabilities::Shadow;
        case EffectId::Glow:
            return EffectCapabilities::Glow;
        case EffectId::Bloom:
            return EffectCapabilities::BackdropCapture | EffectCapabilities::BackdropBlur |
                   EffectCapabilities::Bloom;
        default:
            return EffectCapabilities::None;
        }
    }

    struct EffectVec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct EffectExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    struct EffectRect
    {
        EffectVec2 min{};
        EffectVec2 max{};
    };

    struct EffectColor
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    struct BackgroundShaderParams
    {
        EffectColor topColor{0.018f, 0.026f, 0.050f, 1.0f};
        EffectColor bottomColor{0.003f, 0.006f, 0.014f, 1.0f};
        EffectColor accent{1.0f, 0.24f, 0.36f, 1.0f};
        EffectColor secondary{0.0f, 0.72f, 0.88f, 1.0f};
        float gridSize = 48.0f;
        float gridStrength = 0.42f;
        float vignette = 0.48f;
        float animationSpeed = 1.0f;
        float intensity = 1.25f;
    };

    constexpr EffectColor EffectColorFromRgba8(std::uint32_t rgba) noexcept
    {
        constexpr float scale = 1.0f / 255.0f;
        return {
            static_cast<float>((rgba >> 24) & 0xffu) * scale,
            static_cast<float>((rgba >> 16) & 0xffu) * scale,
            static_cast<float>((rgba >> 8) & 0xffu) * scale,
            static_cast<float>(rgba & 0xffu) * scale
        };
    }

    struct BlurEffectParams
    {
        EffectRect bounds{};
        float sigma = 8.0f;
        float rounding = 0.0f;
        float opacity = 1.0f;
    };

    /** Complete value description for one screen-space glass material. */
    struct GlassEffectParams
    {
        EffectRect bounds{};
        float rounding = 0.0f;
        float opacity = 1.0f;
        EffectColor tint{1.0f, 1.0f, 1.0f, 1.0f};
        float tintStrength = 0.16f;
        EffectColor accent{1.0f, 1.0f, 1.0f, 1.0f};
        float blurSigma = 8.0f;
        float refraction = 1.5f;
        float edgeWidth = 1.5f;
        float edgeHighlight = 0.45f;
        float thickness = 0.5f;
        float shapePower = 2.0f;
        float lensStrength = 0.25f;
        float lensBase = 1.0f;
        float lensFalloff = 4.0f;
        float lensBias = 0.0f;
        float lensPower = 1.0f;
        float noise = 0.0f;
        float glowStrength = 0.0f;
        float glowSpread = 8.0f;
        float shadowStrength = 0.25f;
        float shadowSpread = 12.0f;
    };

    struct ShadowEffectParams
    {
        EffectRect bounds{};
        EffectVec2 offset{0.0f, 6.0f};
        EffectColor color{0.0f, 0.0f, 0.0f, 1.0f};
        float rounding = 0.0f;
        float sigma = 10.0f;
        float strength = 0.35f;
    };

    struct GlowEffectParams
    {
        EffectRect bounds{};
        EffectColor color{1.0f, 1.0f, 1.0f, 1.0f};
        float rounding = 0.0f;
        float sigma = 8.0f;
        float strength = 0.5f;
    };

    struct BloomEffectParams
    {
        EffectRect bounds{};
        float sigma = 8.0f;
        float strength = 0.35f;
        float brightnessThreshold = 0.72f;
        float saturationThreshold = 0.0f;
    };

    enum class ItemEffectType : std::uint32_t
    {
        Shadow,
        Glow,
        Blur
    };

    enum class ItemEffectFlags : std::uint32_t
    {
        None = 0,
        ReplaceSource = 1u << 0
    };

    constexpr ItemEffectFlags operator|(ItemEffectFlags lhs, ItemEffectFlags rhs) noexcept
    {
        return static_cast<ItemEffectFlags>(static_cast<std::uint32_t>(lhs) |
                                            static_cast<std::uint32_t>(rhs));
    }

    constexpr ItemEffectFlags operator&(ItemEffectFlags lhs, ItemEffectFlags rhs) noexcept
    {
        return static_cast<ItemEffectFlags>(static_cast<std::uint32_t>(lhs) &
                                            static_cast<std::uint32_t>(rhs));
    }

    constexpr bool HasItemEffectFlags(ItemEffectFlags available,
                                      ItemEffectFlags required) noexcept
    {
        return (available & required) == required;
    }

    struct ItemEffectLayer
    {
        ItemEffectType type = ItemEffectType::Glow;
        ItemEffectFlags flags = ItemEffectFlags::None;
        EffectVec2 offset{};
        EffectColor color{1.0f, 1.0f, 1.0f, 1.0f};
        float sigma = 8.0f;
        float strength = 0.5f;
        float opacity = 1.0f;

        static constexpr ItemEffectLayer Shadow(EffectColor color, EffectVec2 offset,
                                                float sigma, float strength) noexcept
        {
            return {ItemEffectType::Shadow, ItemEffectFlags::None, offset, color,
                    sigma, strength, 1.0f};
        }

        static constexpr ItemEffectLayer Glow(EffectColor color, float sigma,
                                              float strength) noexcept
        {
            return {ItemEffectType::Glow, ItemEffectFlags::None, {}, color,
                    sigma, strength, 1.0f};
        }

        static constexpr ItemEffectLayer Blur(float sigma, float opacity = 1.0f,
                                              bool replaceSource = true) noexcept
        {
            return {ItemEffectType::Blur,
                    replaceSource ? ItemEffectFlags::ReplaceSource : ItemEffectFlags::None,
                    {}, {}, sigma, 1.0f, opacity};
        }
    };

    /** A bounded stack of effects evaluated from back to front for one item. */
    struct ItemEffectRequest
    {
        std::array<ItemEffectLayer, 4> layers{};
        std::uint32_t layerCount = 0;

        constexpr bool Add(const ItemEffectLayer& layer) noexcept
        {
            if (layerCount >= layers.size())
                return false;
            layers[layerCount++] = layer;
            return true;
        }

        static constexpr ItemEffectRequest From(const ItemEffectLayer& layer) noexcept
        {
            ItemEffectRequest request{};
            request.Add(layer);
            return request;
        }
    };

    struct ItemEffectDrawData;

    /** Move-only scope capturing draw geometry emitted before destruction. */
    class ItemEffectScope
    {
    public:
        ItemEffectScope() noexcept = default;
        ~ItemEffectScope();
        ItemEffectScope(ItemEffectScope&& other) noexcept;
        ItemEffectScope& operator=(ItemEffectScope&& other) noexcept;
        ItemEffectScope(const ItemEffectScope&) = delete;
        ItemEffectScope& operator=(const ItemEffectScope&) = delete;

        explicit operator bool() const noexcept { return active_; }
        void End() noexcept;

    private:
        friend ItemEffectScope BeginItemEffect(ImDrawList*, IEffectRenderer&,
                                                const ItemEffectRequest&);
        ImDrawList* drawList_ = nullptr;
        std::shared_ptr<void> data_;
        std::weak_ptr<void> contextLifetime_;
        void* context_ = nullptr;
        bool active_ = false;
    };

    /** Pointer-free command value copied into ImDrawList callback storage. */
    struct EffectRequest
    {
        EffectId id = EffectId::Glass;
        std::uint32_t flags = 0;
        BlurEffectParams blur{};
        GlassEffectParams glass{};
        ShadowEffectParams shadow{};
        GlowEffectParams glow{};
        BloomEffectParams bloom{};
        std::array<std::byte, 64> customData{};
        std::uint32_t customDataSize = 0;

        static constexpr EffectRequest BackdropBlur(const BlurEffectParams& params) noexcept
        {
            EffectRequest request{};
            request.id = EffectId::BackdropBlur;
            request.blur = params;
            return request;
        }

        static constexpr EffectRequest Glass(const GlassEffectParams& params) noexcept
        {
            EffectRequest request{};
            request.id = EffectId::Glass;
            request.glass = params;
            return request;
        }

        static constexpr EffectRequest Shadow(const ShadowEffectParams& params) noexcept
        {
            EffectRequest request{};
            request.id = EffectId::Shadow;
            request.shadow = params;
            return request;
        }

        static constexpr EffectRequest Glow(const GlowEffectParams& params) noexcept
        {
            EffectRequest request{};
            request.id = EffectId::Glow;
            request.glow = params;
            return request;
        }

        static constexpr EffectRequest Bloom(const BloomEffectParams& params) noexcept
        {
            EffectRequest request{};
            request.id = EffectId::Bloom;
            request.bloom = params;
            return request;
        }
    };

    /** Backend-neutral constant block shared by HLSL and GLSL implementations. */
    struct EffectConstants
    {
        std::array<float, 64> values{};
    };

    constexpr EffectConstants MakeBlurEffectConstants(EffectExtent extent,
                                                       EffectVec2 direction,
                                                       float sigma) noexcept
    {
        EffectConstants constants{};
        constants.values[0] = extent.width != 0 ? 1.0f / static_cast<float>(extent.width) : 0.0f;
        constants.values[1] = extent.height != 0 ? 1.0f / static_cast<float>(extent.height) : 0.0f;
        constants.values[2] = direction.x;
        constants.values[3] = direction.y;
        constants.values[4] = sigma;
        return constants;
    }

    constexpr EffectConstants MakeBackgroundEffectConstants(
        const BackgroundShaderParams& background, EffectExtent extent,
        float timeSeconds) noexcept
    {
        EffectConstants constants{};
        const EffectColor colors[] = {
            background.topColor, background.bottomColor,
            background.accent, background.secondary};
        for (std::size_t index = 0; index < std::size(colors); ++index)
        {
            constants.values[index * 4 + 0] = colors[index].r;
            constants.values[index * 4 + 1] = colors[index].g;
            constants.values[index * 4 + 2] = colors[index].b;
            constants.values[index * 4 + 3] = colors[index].a;
        }
        constants.values[16] = timeSeconds;
        constants.values[17] = background.gridSize;
        constants.values[18] = background.gridStrength;
        constants.values[19] = background.vignette;
        constants.values[20] = static_cast<float>(extent.width);
        constants.values[21] = static_cast<float>(extent.height);
        constants.values[22] = background.animationSpeed;
        constants.values[23] = background.intensity;
        return constants;
    }

    constexpr EffectConstants MakeGlassEffectConstants(const GlassEffectParams& glass,
                                                        EffectExtent extent,
                                                        float timeSeconds) noexcept
    {
        EffectConstants constants{};
        constants.values[0] = glass.bounds.min.x;
        constants.values[1] = glass.bounds.min.y;
        constants.values[2] = glass.bounds.max.x;
        constants.values[3] = glass.bounds.max.y;
        constants.values[4] = glass.rounding;
        constants.values[5] = glass.opacity;
        constants.values[6] = glass.tint.r;
        constants.values[7] = glass.tint.g;
        constants.values[8] = glass.tint.b;
        constants.values[9] = glass.tintStrength;
        constants.values[10] = glass.accent.r;
        constants.values[11] = glass.accent.g;
        constants.values[12] = glass.accent.b;
        constants.values[13] = glass.refraction;
        constants.values[14] = glass.edgeWidth;
        constants.values[15] = glass.edgeHighlight;
        constants.values[16] = glass.thickness;
        constants.values[17] = glass.shapePower;
        constants.values[18] = glass.lensStrength;
        constants.values[19] = glass.lensBase;
        constants.values[20] = glass.lensFalloff;
        constants.values[21] = glass.lensBias;
        constants.values[22] = glass.lensPower;
        constants.values[23] = glass.noise;
        constants.values[24] = glass.glowStrength;
        constants.values[25] = glass.glowSpread;
        constants.values[26] = glass.shadowStrength;
        constants.values[27] = glass.shadowSpread;
        constants.values[28] = extent.width != 0 ? 1.0f / static_cast<float>(extent.width) : 0.0f;
        constants.values[29] = extent.height != 0 ? 1.0f / static_cast<float>(extent.height) : 0.0f;
        constants.values[30] = timeSeconds;
        return constants;
    }

    constexpr EffectConstants MakeBloomEffectConstants(const BloomEffectParams& bloom,
                                                        EffectExtent extent) noexcept
    {
        EffectConstants constants{};
        constants.values[0] = extent.width != 0 ? 1.0f / static_cast<float>(extent.width) : 0.0f;
        constants.values[1] = extent.height != 0 ? 1.0f / static_cast<float>(extent.height) : 0.0f;
        constants.values[4] = bloom.sigma;
        constants.values[5] = bloom.strength;
        constants.values[6] = bloom.brightnessThreshold;
        constants.values[7] = bloom.saturationThreshold;
        constants.values[8] = bloom.bounds.min.x;
        constants.values[9] = bloom.bounds.min.y;
        constants.values[10] = bloom.bounds.max.x;
        constants.values[11] = bloom.bounds.max.y;
        return constants;
    }

    static_assert(std::is_standard_layout_v<EffectRequest> &&
                  std::is_trivially_copyable_v<EffectRequest>);
    static_assert(std::is_standard_layout_v<EffectConstants> &&
                  std::is_trivially_copyable_v<EffectConstants> &&
                  sizeof(EffectConstants) == sizeof(float) * 64);

    class IEffectRenderer
    {
    public:
        virtual ~IEffectRenderer() = default;
        virtual EffectCapabilities GetCapabilities() const noexcept = 0;
        virtual void RenderEffect(const EffectRequest& request) noexcept = 0;
        virtual void RenderItemEffects(const ItemEffectRequest& request,
                                       const ItemEffectDrawData& drawData) noexcept
        {
            (void)request;
            (void)drawData;
        }
        virtual IEffectRenderer* GetBackendRenderer() noexcept { return this; }
    };

    using CustomEffectCallback = void (*)(void* userData, const EffectRequest& request,
                                          IEffectRenderer& backend) noexcept;

    /** Dispatches custom effect IDs before forwarding built-in effects to a backend. */
    class EffectRegistry final : public IEffectRenderer
    {
    public:
        explicit EffectRegistry(IEffectRenderer* backend = nullptr);
        ~EffectRegistry() override;
        EffectRegistry(EffectRegistry&&) noexcept;
        EffectRegistry& operator=(EffectRegistry&&) noexcept;
        EffectRegistry(const EffectRegistry&) = delete;
        EffectRegistry& operator=(const EffectRegistry&) = delete;

        bool Register(EffectId id, CustomEffectCallback callback, void* userData = nullptr);
        bool Unregister(EffectId id);
        void SetBackend(IEffectRenderer* backend) noexcept;
        EffectCapabilities GetCapabilities() const noexcept override;
        void RenderEffect(const EffectRequest& request) noexcept override;
        void RenderItemEffects(const ItemEffectRequest& request,
                               const ItemEffectDrawData& drawData) noexcept override;
        IEffectRenderer* GetBackendRenderer() noexcept override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

#ifdef IVUI_INTERNAL_API
    /** Adds an effect at the current draw-list Z position and then resets ImGui state.
     *  The renderer is borrowed and must outlive rendering of the draw data.
     */
    void SubmitEffect(ImDrawList* drawList, IEffectRenderer& renderer,
                      const EffectRequest& request);

    /** Convenience overload using ImGui::GetWindowDrawList(). */
    void SubmitEffect(IEffectRenderer& renderer, const EffectRequest& request);
#endif

    /** Begins capturing geometry subsequently emitted to @p drawList. */
    ItemEffectScope BeginItemEffect(ImDrawList* drawList, IEffectRenderer& renderer,
                                    const ItemEffectRequest& request);

    /** Convenience overload using ImGui::GetWindowDrawList(). */
    ItemEffectScope BeginItemEffect(IEffectRenderer& renderer,
                                    const ItemEffectRequest& request);
}
