#include "ImGuiVectorUI/Backends/RendererVulkan.h"

#include "imgui_impl_vulkan.h"

namespace IVUI::Vulkan
{
    bool RendererBackend::Initialize(ImGuiContext&, std::string& error) noexcept
    {
        if (!info_.instance || !info_.physicalDevice || !info_.device || !info_.queue ||
            !info_.renderPass || info_.imageCount < 2 || info_.minImageCount < 2)
        {
            error = "Vulkan renderer create info is incomplete.";
            return false;
        }
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = info_.apiVersion;
        info.Instance = info_.instance;
        info.PhysicalDevice = info_.physicalDevice;
        info.Device = info_.device;
        info.QueueFamily = info_.queueFamily;
        info.Queue = info_.queue;
        info.DescriptorPool = info_.descriptorPool;
        info.DescriptorPoolSize = info_.descriptorPoolSize;
        info.MinImageCount = info_.minImageCount;
        info.ImageCount = info_.imageCount;
        info.PipelineCache = info_.pipelineCache;
        info.PipelineInfoMain.RenderPass = info_.renderPass;
        info.PipelineInfoMain.Subpass = info_.subpass;
        info.PipelineInfoMain.MSAASamples = info_.samples;
        info.Allocator = info_.allocator;
        info.CheckVkResultFn = info_.checkResult;
        if (!ImGui_ImplVulkan_Init(&info))
        {
            error = "Vulkan renderer initialization failed.";
            return false;
        }
        initialized_ = true;
        return true;
    }

    void RendererBackend::Shutdown() noexcept
    {
        if (initialized_) ImGui_ImplVulkan_Shutdown();
        initialized_ = false;
    }

    void RendererBackend::NewFrame() noexcept
    {
        if (initialized_) ImGui_ImplVulkan_NewFrame();
    }

    bool RendererBackend::RenderDrawData(ImDrawData& drawData, const IVUI::RenderFrame& base,
                                         IEffectRenderer* effects, std::string& error) noexcept
    {
        const auto* frame = dynamic_cast<const RenderFrame*>(&base);
        if (!initialized_ || !frame || !frame->effects.commandBuffer)
        {
            error = "Vulkan renderer received an incompatible frame.";
            return false;
        }
        auto* effectRenderer = dynamic_cast<EffectRenderer*>(effects ? effects->GetBackendRenderer() : nullptr);
        if (effectRenderer && !effectRenderer->PrepareFrame(frame->effects))
        {
            error = effectRenderer->GetLastError();
            return false;
        }
        if (frame->beginRenderPass)
            frame->beginRenderPass(frame->userData, frame->effects.commandBuffer);
        if (effectRenderer && !effectRenderer->BeginFrame(frame->effects))
        {
            error = effectRenderer->GetLastError();
            if (frame->endRenderPass)
                frame->endRenderPass(frame->userData, frame->effects.commandBuffer);
            return false;
        }
        ImGui_ImplVulkan_RenderDrawData(&drawData, frame->effects.commandBuffer, frame->pipeline);
        if (effectRenderer) effectRenderer->EndFrame();
        if (frame->endRenderPass)
            frame->endRenderPass(frame->userData, frame->effects.commandBuffer);
        return true;
    }
}
