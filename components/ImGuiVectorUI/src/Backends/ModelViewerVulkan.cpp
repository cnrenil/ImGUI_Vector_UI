#include "ImGuiVectorUI/Backends/ModelViewerVulkan.h"

#include <algorithm>
#include <cstring>

namespace IVUI::Vulkan
{
    namespace
    {
        void SetError(std::string* error, const char* message)
        {
            if (error)
                *error = message ? message : "ModelViewer Vulkan texture bridge failed.";
        }

        ImTextureID ToTextureId(VkDescriptorSet descriptorSet) noexcept
        {
            return static_cast<ImTextureID>(
                reinterpret_cast<std::uintptr_t>(descriptorSet));
        }

        bool Check(VkResult result, std::string* error, const char* message)
        {
            if (result == VK_SUCCESS)
                return true;
            SetError(error, message);
            return false;
        }
    }

    ModelViewerTextureBridge::ModelViewerTextureBridge(
        ModelViewerTextureBridgeCreateInfo info) noexcept : info_(info) {}

    ModelViewerTextureBridge::~ModelViewerTextureBridge()
    {
        ResetAfterDeviceIdle();
    }

    bool ModelViewerTextureBridge::Initialize(std::string* error) noexcept
    {
        if (!info_.physicalDevice || !info_.device || !info_.descriptorPool)
        {
            SetError(error, "ModelViewer Vulkan requires a physical device, device and descriptor pool.");
            return false;
        }
        if (!descriptorSetLayout_)
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo layoutInfo{
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &binding;
            if (!Check(vkCreateDescriptorSetLayout(info_.device, &layoutInfo, info_.allocator,
                                                   &descriptorSetLayout_), error,
                       "Creating the ModelViewer Vulkan descriptor layout failed."))
                return false;
        }
        initialized_ = true;
        if (error)
            error->clear();
        return true;
    }

    std::uint32_t ModelViewerTextureBridge::FindMemoryType(
        std::uint32_t typeBits, VkMemoryPropertyFlags properties) const noexcept
    {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(info_.physicalDevice, &memoryProperties);
        for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
            if ((typeBits & (1u << index)) != 0 &&
                (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties)
                return index;
        return UINT32_MAX;
    }

    bool ModelViewerTextureBridge::CreateResources(int width, int height,
                                                   std::string* error) noexcept
    {
        if (width <= 0 || height <= 0)
        {
            SetError(error, "ModelViewer Vulkan image dimensions are invalid.");
            return false;
        }

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {static_cast<std::uint32_t>(width),
                            static_cast<std::uint32_t>(height), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!Check(vkCreateImage(info_.device, &imageInfo, info_.allocator, &image_), error,
                   "Creating the ModelViewer Vulkan image failed."))
            return false;

        VkMemoryRequirements imageRequirements{};
        vkGetImageMemoryRequirements(info_.device, image_, &imageRequirements);
        const std::uint32_t imageMemoryType = FindMemoryType(
            imageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (imageMemoryType == UINT32_MAX)
        {
            SetError(error, "No device-local Vulkan memory type is available for ModelViewer.");
            ReleaseResources();
            return false;
        }
        VkMemoryAllocateInfo imageAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        imageAllocation.allocationSize = imageRequirements.size;
        imageAllocation.memoryTypeIndex = imageMemoryType;
        if (!Check(vkAllocateMemory(info_.device, &imageAllocation, info_.allocator,
                                    &imageMemory_), error,
                   "Allocating ModelViewer Vulkan image memory failed.") ||
            !Check(vkBindImageMemory(info_.device, image_, imageMemory_, 0), error,
                   "Binding ModelViewer Vulkan image memory failed."))
        {
            ReleaseResources();
            return false;
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (!Check(vkCreateImageView(info_.device, &viewInfo, info_.allocator, &imageView_),
                   error, "Creating the ModelViewer Vulkan image view failed."))
        {
            ReleaseResources();
            return false;
        }

        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 1.0f;
        if (!Check(vkCreateSampler(info_.device, &samplerInfo, info_.allocator, &sampler_),
                   error, "Creating the ModelViewer Vulkan sampler failed."))
        {
            ReleaseResources();
            return false;
        }

        VkDescriptorSetAllocateInfo descriptorInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        descriptorInfo.descriptorPool = info_.descriptorPool;
        descriptorInfo.descriptorSetCount = 1;
        descriptorInfo.pSetLayouts = &descriptorSetLayout_;
        if (!Check(vkAllocateDescriptorSets(info_.device, &descriptorInfo, &descriptorSet_),
                   error, "Allocating the ModelViewer Vulkan descriptor failed."))
        {
            ReleaseResources();
            return false;
        }

        const VkDeviceSize size = static_cast<VkDeviceSize>(width) *
                                  static_cast<VkDeviceSize>(height) * 4u;
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (!Check(vkCreateBuffer(info_.device, &bufferInfo, info_.allocator, &stagingBuffer_),
                   error, "Creating the ModelViewer Vulkan staging buffer failed."))
        {
            ReleaseResources();
            return false;
        }
        VkMemoryRequirements stagingRequirements{};
        vkGetBufferMemoryRequirements(info_.device, stagingBuffer_, &stagingRequirements);
        const std::uint32_t stagingMemoryType = FindMemoryType(
            stagingRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (stagingMemoryType == UINT32_MAX)
        {
            SetError(error, "No host-visible Vulkan memory type is available for ModelViewer.");
            ReleaseResources();
            return false;
        }
        VkMemoryAllocateInfo stagingAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        stagingAllocation.allocationSize = stagingRequirements.size;
        stagingAllocation.memoryTypeIndex = stagingMemoryType;
        if (!Check(vkAllocateMemory(info_.device, &stagingAllocation, info_.allocator,
                                    &stagingMemory_), error,
                   "Allocating ModelViewer Vulkan staging memory failed.") ||
            !Check(vkBindBufferMemory(info_.device, stagingBuffer_, stagingMemory_, 0), error,
                   "Binding ModelViewer Vulkan staging memory failed."))
        {
            ReleaseResources();
            return false;
        }

        VkDescriptorImageInfo descriptorImage{};
        descriptorImage.sampler = sampler_;
        descriptorImage.imageView = imageView_;
        descriptorImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descriptorSet_;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &descriptorImage;
        vkUpdateDescriptorSets(info_.device, 1, &write, 0, nullptr);
        width_ = width;
        height_ = height;
        imageInitialized_ = false;
        return true;
    }

    ImTextureID ModelViewerTextureBridge::Upload(const ModelViewerOffscreenImage& image,
                                                 VkCommandBuffer commandBuffer,
                                                 std::string* error) noexcept
    {
        if (!initialized_ && !Initialize(error))
            return ImTextureID_Invalid;
        if (!commandBuffer || !image.Valid())
        {
            SetError(error, "ModelViewer Vulkan requires a command buffer and valid RGBA image.");
            return ImTextureID_Invalid;
        }
        if (image_ && (width_ != image.width || height_ != image.height))
        {
            SetError(error, "ModelViewer Vulkan size changes require ResetAfterDeviceIdle first.");
            return ImTextureID_Invalid;
        }
        if (!image_ && !CreateResources(image.width, image.height, error))
            return ImTextureID_Invalid;

        void* mapped = nullptr;
        if (!Check(vkMapMemory(info_.device, stagingMemory_, 0, VK_WHOLE_SIZE, 0, &mapped),
                   error, "Mapping the ModelViewer Vulkan staging memory failed."))
            return ImTextureID_Invalid;
        const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4u;
        for (int row = 0; row < image.height; ++row)
            std::memcpy(static_cast<std::uint8_t*>(mapped) +
                            static_cast<std::size_t>(row) * rowBytes,
                        image.pixels.data() + static_cast<std::size_t>(row) * image.strideBytes,
                        rowBytes);
        vkUnmapMemory(info_.device, stagingMemory_);

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = imageInitialized_ ? VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = imageInitialized_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                               : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image_;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, imageInitialized_ ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                               : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {static_cast<std::uint32_t>(image.width),
                            static_cast<std::uint32_t>(image.height), 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer_, image_,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
        imageInitialized_ = true;
        if (error)
            error->clear();
        return TextureId();
    }

    void ModelViewerTextureBridge::ReleaseResources() noexcept
    {
        if (descriptorSet_ && info_.device && info_.descriptorPool)
            vkFreeDescriptorSets(info_.device, info_.descriptorPool, 1, &descriptorSet_);
        if (stagingBuffer_)
            vkDestroyBuffer(info_.device, stagingBuffer_, info_.allocator);
        if (stagingMemory_)
            vkFreeMemory(info_.device, stagingMemory_, info_.allocator);
        if (imageView_)
            vkDestroyImageView(info_.device, imageView_, info_.allocator);
        if (sampler_)
            vkDestroySampler(info_.device, sampler_, info_.allocator);
        if (image_)
            vkDestroyImage(info_.device, image_, info_.allocator);
        if (imageMemory_)
            vkFreeMemory(info_.device, imageMemory_, info_.allocator);
        descriptorSet_ = VK_NULL_HANDLE;
        stagingBuffer_ = VK_NULL_HANDLE;
        stagingMemory_ = VK_NULL_HANDLE;
        imageView_ = VK_NULL_HANDLE;
        sampler_ = VK_NULL_HANDLE;
        image_ = VK_NULL_HANDLE;
        imageMemory_ = VK_NULL_HANDLE;
        width_ = height_ = 0;
        imageInitialized_ = false;
    }

    void ModelViewerTextureBridge::ResetAfterDeviceIdle() noexcept
    {
        if (info_.device)
            ReleaseResources();
        if (descriptorSetLayout_ && info_.device)
            vkDestroyDescriptorSetLayout(info_.device, descriptorSetLayout_, info_.allocator);
        descriptorSetLayout_ = VK_NULL_HANDLE;
        initialized_ = false;
    }

    ImTextureID ModelViewerTextureBridge::TextureId() const noexcept
    {
        return ToTextureId(descriptorSet_);
    }

    ImTextureID ModelViewerTextureBridge::UploadCallback(
        const ModelViewerOffscreenImage& image, void* userData) noexcept
    {
        auto* callback = static_cast<ModelViewerTextureBridgeCallbackContext*>(userData);
        return callback && callback->bridge
            ? callback->bridge->Upload(image, callback->commandBuffer)
            : ImTextureID_Invalid;
    }
}
