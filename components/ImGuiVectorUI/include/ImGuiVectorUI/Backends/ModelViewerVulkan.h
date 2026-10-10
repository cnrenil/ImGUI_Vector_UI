/** @file ModelViewerVulkan.h @brief Vulkan texture bridge for ModelViewer. */
#pragma once

#include "ImGuiVectorUI/Optional/ModelViewer.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace IVUI::Vulkan
{
    struct ModelViewerTextureBridgeCreateInfo
    {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        const VkAllocationCallbacks* allocator = nullptr;
    };

    struct ModelViewerTextureBridgeCallbackContext
    {
        class ModelViewerTextureBridge* bridge = nullptr;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    };

    class ModelViewerTextureBridge final
    {
    public:
        explicit ModelViewerTextureBridge(ModelViewerTextureBridgeCreateInfo info) noexcept;
        ~ModelViewerTextureBridge();
        ModelViewerTextureBridge(const ModelViewerTextureBridge&) = delete;
        ModelViewerTextureBridge& operator=(const ModelViewerTextureBridge&) = delete;

        bool Initialize(std::string* error = nullptr) noexcept;
        ImTextureID Upload(const ModelViewerOffscreenImage& image,
                           VkCommandBuffer commandBuffer,
                           std::string* error = nullptr) noexcept;
        /** Call only after the host has waited for all upload command buffers. */
        void ResetAfterDeviceIdle() noexcept;

        ImTextureID TextureId() const noexcept;
        VkDescriptorSet DescriptorSet() const noexcept { return descriptorSet_; }

        static ImTextureID UploadCallback(const ModelViewerOffscreenImage& image,
                                          void* userData) noexcept;

    private:
        bool CreateResources(int width, int height, std::string* error) noexcept;
        std::uint32_t FindMemoryType(std::uint32_t typeBits,
                                     VkMemoryPropertyFlags properties) const noexcept;
        void ReleaseResources() noexcept;

        ModelViewerTextureBridgeCreateInfo info_{};
        VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
        int width_ = 0;
        int height_ = 0;
        bool initialized_ = false;
        bool imageInitialized_ = false;
    };
}
