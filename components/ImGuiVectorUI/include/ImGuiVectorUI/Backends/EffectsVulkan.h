/** @file EffectsVulkan.h
 *  @brief Vulkan implementation boundary for the IVUI effect renderer.
 */
#pragma once

#include "ImGuiVectorUI/Drawing/Effects.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>

namespace IVUI::Vulkan
{
    struct EffectRendererCreateInfo
    {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkFormat targetFormat = VK_FORMAT_UNDEFINED;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::uint32_t subpass = 0;
        std::uint32_t framesInFlight = 2;
        const VkAllocationCallbacks* allocator = nullptr;
        /** Advertises item effects. Frames using them must enable target capture. */
        bool enableItemEffects = false;
    };

    struct EffectRendererFrameInfo
    {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        const ImDrawData* drawData = nullptr;
        VkImage targetImage = VK_NULL_HANDLE;
        VkImageView targetView = VK_NULL_HANDLE;
        VkImageLayout targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkImage backdropImage = VK_NULL_HANDLE;
        VkImageView backdropView = VK_NULL_HANDLE;
        VkImageLayout backdropLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        EffectExtent extent{};
        /** Completed frame-resource slot. The host must wait for its fence before reuse. */
        std::uint32_t frameIndex = UINT32_MAX;
        float timeSeconds = 0.0f;
        float backdropBlurSigma = 8.0f;
        bool renderBackground = false;
        BackgroundShaderParams background{};
        /** Enables draw-depth target capture. The active render pass must use a
         *  single-sampled color attachment in subpass 0, have no depth/resolve
         *  attachment, be compatible with a one-color load pass, and finish in PRESENT.
         */
        bool allowTargetCapture = false;
        bool renderBloom = false;
        BloomEffectParams bloom{};
    };

    class EffectRenderer final : public IEffectRenderer
    {
    public:
        static std::unique_ptr<EffectRenderer> Create(
            const EffectRendererCreateInfo& createInfo,
            std::string* error = nullptr) noexcept;

        ~EffectRenderer() override;
        EffectRenderer(EffectRenderer&&) noexcept;
        EffectRenderer& operator=(EffectRenderer&&) noexcept;
        EffectRenderer(const EffectRenderer&) = delete;
        EffectRenderer& operator=(const EffectRenderer&) = delete;

        /** Records capture/blur work before the host begins its ImGui render pass. */
        bool PrepareFrame(const EffectRendererFrameInfo& frameInfo) noexcept;
        /** Selects the frame consumed by subsequent draw-list callbacks. */
        bool BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept;
        void EndFrame() noexcept;
        const char* GetLastError() const noexcept;

        EffectCapabilities GetCapabilities() const noexcept override;
        void RenderEffect(const EffectRequest& request) noexcept override;
        void RenderItemEffects(const ItemEffectRequest& request,
                               const ItemEffectDrawData& drawData) noexcept override;

    private:
        struct Impl;
        explicit EffectRenderer(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> impl_;
    };
}
