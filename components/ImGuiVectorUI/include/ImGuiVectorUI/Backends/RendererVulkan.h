/** @file RendererVulkan.h @brief Dear ImGui Vulkan renderer adapter. */
#pragma once

#include "ImGuiVectorUI/UI.h"
#include "ImGuiVectorUI/Backends/EffectsVulkan.h"

namespace IVUI::Vulkan
{
    struct RendererCreateInfo
    {
        std::uint32_t apiVersion = VK_API_VERSION_1_0;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t queueFamily = 0;
        VkQueue queue = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::uint32_t descriptorPoolSize = 0;
        std::uint32_t minImageCount = 2;
        std::uint32_t imageCount = 2;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::uint32_t subpass = 0;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        const VkAllocationCallbacks* allocator = nullptr;
        void (*checkResult)(VkResult) = nullptr;
    };

    using RenderPassCallback = void (*)(void* userData, VkCommandBuffer commandBuffer);

    struct RenderFrame final : IVUI::RenderFrame
    {
        EffectRendererFrameInfo effects{};
        VkPipeline pipeline = VK_NULL_HANDLE;
        RenderPassCallback beginRenderPass = nullptr;
        RenderPassCallback endRenderPass = nullptr;
        void* userData = nullptr;
    };

    class RendererBackend final : public IRendererBackend
    {
    public:
        explicit RendererBackend(RendererCreateInfo info) noexcept : info_(info) {}
        bool Initialize(ImGuiContext& context, std::string& error) noexcept override;
        void Shutdown() noexcept override;
        void NewFrame() noexcept override;
        bool RenderDrawData(ImDrawData& drawData, const IVUI::RenderFrame& frame,
                            IEffectRenderer* effects, std::string& error) noexcept override;

    private:
        RendererCreateInfo info_{};
        bool initialized_ = false;
    };
}
