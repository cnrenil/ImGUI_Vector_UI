#include "ImGuiVectorUI/Backends/EffectsVulkan.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace IVUI::Vulkan
{
    struct EffectShaderLibrary::Impl
    {
        std::array<std::vector<std::uint32_t>, 5> binaries;
    };

    std::unique_ptr<EffectShaderLibrary> EffectShaderLibrary::Load(
        const char* directory, std::string* error) noexcept
    {
        try
        {
            if (!directory || !*directory)
            {
                if (error) *error = "A Vulkan shader directory is required.";
                return nullptr;
            }
            auto impl = std::make_unique<Impl>();
            constexpr const char* names[] = {
                "effects.vert.spv", "effects_blur.frag.spv", "effects_material.frag.spv",
                "effects_shape.frag.spv", "effects_bloom.frag.spv"};
            for (std::size_t index = 0; index < std::size(names); ++index)
            {
                const auto path = std::filesystem::path(directory) / names[index];
                std::ifstream input(path, std::ios::binary | std::ios::ate);
                if (!input)
                {
                    if (error) *error = "Unable to open Vulkan effect shader: " + path.string();
                    return nullptr;
                }
                const std::streamsize bytes = input.tellg();
                if (bytes <= 0 || bytes % static_cast<std::streamsize>(sizeof(std::uint32_t)) != 0)
                {
                    if (error) *error = "Invalid SPIR-V byte size: " + path.string();
                    return nullptr;
                }
                input.seekg(0);
                auto& output = impl->binaries[index];
                output.resize(static_cast<std::size_t>(bytes) / sizeof(std::uint32_t));
                if (!input.read(reinterpret_cast<char*>(output.data()), bytes))
                {
                    if (error) *error = "Unable to read Vulkan effect shader: " + path.string();
                    return nullptr;
                }
            }
            if (error) error->clear();
            return std::unique_ptr<EffectShaderLibrary>(new EffectShaderLibrary(std::move(impl)));
        }
        catch (const std::exception& exception)
        {
            if (error) *error = exception.what();
            return nullptr;
        }
    }

    EffectShaderLibrary::EffectShaderLibrary(std::unique_ptr<Impl> impl) noexcept
        : impl_(std::move(impl)) {}
    EffectShaderLibrary::~EffectShaderLibrary() = default;
    EffectShaderLibrary::EffectShaderLibrary(EffectShaderLibrary&&) noexcept = default;
    EffectShaderLibrary& EffectShaderLibrary::operator=(EffectShaderLibrary&&) noexcept = default;
    EffectShaderSet EffectShaderLibrary::Get() const noexcept
    {
        if (!impl_) return {};
        const auto shader = [this](std::size_t index) {
            const auto& binary = impl_->binaries[index];
            return ShaderBinary{binary.data(), binary.size() * sizeof(std::uint32_t)};
        };
        return {shader(0), shader(1), shader(2), shader(3), shader(4)};
    }

    namespace
    {
        constexpr std::uint32_t kPushConstantSize = sizeof(float) * 32;

        bool ValidShader(const ShaderBinary& shader)
        {
            return shader.code != nullptr && shader.sizeBytes != 0 &&
                   shader.sizeBytes % sizeof(std::uint32_t) == 0;
        }

        EffectConstants MakeShapeConstants(const EffectRect& bounds, float rounding,
                                           float strength, float sigma, EffectColor color,
                                           EffectVec2 offset)
        {
            EffectConstants constants{};
            constants.values[0] = bounds.min.x;
            constants.values[1] = bounds.min.y;
            constants.values[2] = bounds.max.x;
            constants.values[3] = bounds.max.y;
            constants.values[4] = rounding;
            constants.values[5] = strength;
            constants.values[6] = sigma;
            constants.values[8] = color.r;
            constants.values[9] = color.g;
            constants.values[10] = color.b;
            constants.values[12] = offset.x;
            constants.values[13] = offset.y;
            return constants;
        }
    }

    struct EffectRenderer::Impl
    {
        struct Image
        {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
        };

        struct FrameResources
        {
            Image source{};
            Image ping{};
            Image pong{};
            VkFramebuffer pingFramebuffer = VK_NULL_HANDLE;
            VkFramebuffer pongFramebuffer = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, 3> descriptorSets{};
            bool sourceInitialized = false;
            bool prepared = false;
        };

        explicit Impl(const EffectRendererCreateInfo& info) : createInfo(info) {}

        EffectRendererCreateInfo createInfo{};
        EffectRendererFrameInfo frame{};
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
        VkPipeline blurPipeline = VK_NULL_HANDLE;
        VkPipeline materialPipeline = VK_NULL_HANDLE;
        VkPipeline shapePipeline = VK_NULL_HANDLE;
        VkPipeline bloomPipeline = VK_NULL_HANDLE;
        std::vector<FrameResources> frames;
        EffectExtent resourceExtent{};
        bool frameActive = false;
        std::string error;

        ~Impl()
        {
            DestroyResources();
            const VkDevice device = createInfo.device;
            if (bloomPipeline) vkDestroyPipeline(device, bloomPipeline, createInfo.allocator);
            if (shapePipeline) vkDestroyPipeline(device, shapePipeline, createInfo.allocator);
            if (materialPipeline) vkDestroyPipeline(device, materialPipeline, createInfo.allocator);
            if (blurPipeline) vkDestroyPipeline(device, blurPipeline, createInfo.allocator);
            if (offscreenRenderPass) vkDestroyRenderPass(device, offscreenRenderPass, createInfo.allocator);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, createInfo.allocator);
            if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, createInfo.allocator);
            if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, createInfo.allocator);
            if (sampler) vkDestroySampler(device, sampler, createInfo.allocator);
        }

        bool Check(VkResult result, const char* message)
        {
            if (result == VK_SUCCESS)
                return true;
            error = message;
            return false;
        }

        std::uint32_t FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags)
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(createInfo.physicalDevice, &properties);
            for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
                if ((bits & (1u << index)) != 0 &&
                    (properties.memoryTypes[index].propertyFlags & flags) == flags)
                    return index;
            return UINT32_MAX;
        }

        VkShaderModule CreateShaderModule(const ShaderBinary& shader)
        {
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize = shader.sizeBytes;
            info.pCode = shader.code;
            VkShaderModule module = VK_NULL_HANDLE;
            if (!Check(vkCreateShaderModule(createInfo.device, &info, createInfo.allocator, &module),
                       "Creating a Vulkan effect shader module failed."))
                return VK_NULL_HANDLE;
            return module;
        }

        bool Initialize()
        {
            if (createInfo.physicalDevice == VK_NULL_HANDLE || createInfo.device == VK_NULL_HANDLE ||
                createInfo.targetFormat == VK_FORMAT_UNDEFINED || createInfo.renderPass == VK_NULL_HANDLE ||
                createInfo.framesInFlight == 0 || !ValidShader(createInfo.shaders.fullscreenVertex) ||
                !ValidShader(createInfo.shaders.blurFragment) ||
                !ValidShader(createInfo.shaders.materialFragment) ||
                 !ValidShader(createInfo.shaders.shapeFragment) ||
                 !ValidShader(createInfo.shaders.bloomFragment))
            {
                error = "Vulkan effects require a device, render pass, format and five SPIR-V shaders.";
                return false;
            }

            VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW =
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.maxLod = 1.0f;
            if (!Check(vkCreateSampler(createInfo.device, &samplerInfo, createInfo.allocator, &sampler),
                       "Creating the Vulkan effect sampler failed."))
                return false;

            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo setLayoutInfo{
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setLayoutInfo.bindingCount = 1;
            setLayoutInfo.pBindings = &binding;
            if (!Check(vkCreateDescriptorSetLayout(createInfo.device, &setLayoutInfo,
                                                    createInfo.allocator, &descriptorSetLayout),
                       "Creating the Vulkan effect descriptor layout failed."))
                return false;

            VkPushConstantRange pushRange{};
            pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRange.size = kPushConstantSize;
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
            if (!Check(vkCreatePipelineLayout(createInfo.device, &pipelineLayoutInfo,
                                               createInfo.allocator, &pipelineLayout),
                       "Creating the Vulkan effect pipeline layout failed."))
                return false;

            VkAttachmentDescription attachment{};
            attachment.format = createInfo.targetFormat;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorReference;
            VkSubpassDependency dependencies[2]{};
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &attachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 2;
            renderPassInfo.pDependencies = dependencies;
            if (!Check(vkCreateRenderPass(createInfo.device, &renderPassInfo, createInfo.allocator,
                                          &offscreenRenderPass),
                       "Creating the Vulkan effect offscreen render pass failed."))
                return false;

            VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          createInfo.framesInFlight * 3};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = createInfo.framesInFlight * 3;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            if (!Check(vkCreateDescriptorPool(createInfo.device, &poolInfo, createInfo.allocator,
                                               &descriptorPool),
                       "Creating the Vulkan effect descriptor pool failed."))
                return false;

            if (!CreatePipelines())
                return false;
            frames.resize(createInfo.framesInFlight);
            return true;
        }

        VkPipeline CreatePipeline(VkShaderModule vertexModule, VkShaderModule fragmentModule,
                                  VkRenderPass renderPass, std::uint32_t subpass,
                                   bool alphaBlend, bool additive = false)
        {
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vertexModule;
            stages[0].pName = "main";
            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragmentModule;
            stages[1].pName = "main";
            VkPipelineVertexInputStateCreateInfo vertexInput{
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = 1;
            viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo rasterization{
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization.cullMode = VK_CULL_MODE_NONE;
            rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample{
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blendAttachment{};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachment.blendEnable = alphaBlend;
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo blend{
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;
            const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = 2;
            info.pStages = stages;
            info.pVertexInputState = &vertexInput;
            info.pInputAssemblyState = &assembly;
            info.pViewportState = &viewport;
            info.pRasterizationState = &rasterization;
            info.pMultisampleState = &multisample;
            info.pColorBlendState = &blend;
            info.pDynamicState = &dynamic;
            info.layout = pipelineLayout;
            info.renderPass = renderPass;
            info.subpass = subpass;
            VkPipeline pipeline = VK_NULL_HANDLE;
            if (!Check(vkCreateGraphicsPipelines(createInfo.device, VK_NULL_HANDLE, 1, &info,
                                                  createInfo.allocator, &pipeline),
                       "Creating a Vulkan effect graphics pipeline failed."))
                return VK_NULL_HANDLE;
            return pipeline;
        }

        bool CreatePipelines()
        {
            VkShaderModule vertex = CreateShaderModule(createInfo.shaders.fullscreenVertex);
            VkShaderModule blur = CreateShaderModule(createInfo.shaders.blurFragment);
            VkShaderModule material = CreateShaderModule(createInfo.shaders.materialFragment);
            VkShaderModule shape = CreateShaderModule(createInfo.shaders.shapeFragment);
            VkShaderModule bloom = CreateShaderModule(createInfo.shaders.bloomFragment);
            if (!vertex || !blur || !material || !shape || !bloom)
            {
                if (bloom) vkDestroyShaderModule(createInfo.device, bloom, createInfo.allocator);
                if (shape) vkDestroyShaderModule(createInfo.device, shape, createInfo.allocator);
                if (material) vkDestroyShaderModule(createInfo.device, material, createInfo.allocator);
                if (blur) vkDestroyShaderModule(createInfo.device, blur, createInfo.allocator);
                if (vertex) vkDestroyShaderModule(createInfo.device, vertex, createInfo.allocator);
                return false;
            }
            blurPipeline = CreatePipeline(vertex, blur, offscreenRenderPass, 0, false);
            materialPipeline = CreatePipeline(vertex, material, createInfo.renderPass,
                                              createInfo.subpass, true);
            shapePipeline = CreatePipeline(vertex, shape, createInfo.renderPass,
                                            createInfo.subpass, true);
            bloomPipeline = CreatePipeline(vertex, bloom, createInfo.renderPass,
                                            createInfo.subpass, true, true);
            vkDestroyShaderModule(createInfo.device, bloom, createInfo.allocator);
            vkDestroyShaderModule(createInfo.device, shape, createInfo.allocator);
            vkDestroyShaderModule(createInfo.device, material, createInfo.allocator);
            vkDestroyShaderModule(createInfo.device, blur, createInfo.allocator);
            vkDestroyShaderModule(createInfo.device, vertex, createInfo.allocator);
            return blurPipeline && materialPipeline && shapePipeline && bloomPipeline;
        }

        void DestroyImage(Image& image)
        {
            if (image.view) vkDestroyImageView(createInfo.device, image.view, createInfo.allocator);
            if (image.image) vkDestroyImage(createInfo.device, image.image, createInfo.allocator);
            if (image.memory) vkFreeMemory(createInfo.device, image.memory, createInfo.allocator);
            image = {};
        }

        void DestroyResources()
        {
            for (auto& resources : frames)
            {
                if (resources.pingFramebuffer)
                    vkDestroyFramebuffer(createInfo.device, resources.pingFramebuffer, createInfo.allocator);
                if (resources.pongFramebuffer)
                    vkDestroyFramebuffer(createInfo.device, resources.pongFramebuffer, createInfo.allocator);
                DestroyImage(resources.source);
                DestroyImage(resources.ping);
                DestroyImage(resources.pong);
                resources = {};
            }
            resourceExtent = {};
        }

        bool CreateImage(VkImageUsageFlags usage, Image& image)
        {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = createInfo.targetFormat;
            info.extent = {resourceExtent.width, resourceExtent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (!Check(vkCreateImage(createInfo.device, &info, createInfo.allocator, &image.image),
                       "Creating a Vulkan effect image failed."))
                return false;
            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(createInfo.device, image.image, &requirements);
            const std::uint32_t memoryType = FindMemoryType(requirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (memoryType == UINT32_MAX)
            {
                error = "No device-local memory type is available for Vulkan effects.";
                return false;
            }
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = memoryType;
            if (!Check(vkAllocateMemory(createInfo.device, &allocation, createInfo.allocator,
                                        &image.memory),
                       "Allocating Vulkan effect image memory failed.") ||
                !Check(vkBindImageMemory(createInfo.device, image.image, image.memory, 0),
                       "Binding Vulkan effect image memory failed."))
                return false;
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = createInfo.targetFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            return Check(vkCreateImageView(createInfo.device, &viewInfo, createInfo.allocator,
                                           &image.view),
                         "Creating a Vulkan effect image view failed.");
        }

        bool EnsureResources(EffectExtent extent)
        {
            if (!frames.empty() && frames[0].source.image && resourceExtent.width == extent.width &&
                resourceExtent.height == extent.height)
                return true;
            DestroyResources();
            vkResetDescriptorPool(createInfo.device, descriptorPool, 0);
            resourceExtent = extent;
            frames.resize(createInfo.framesInFlight);
            for (auto& resources : frames)
            {
                if (!CreateImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 resources.source) ||
                    !CreateImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 resources.ping) ||
                    !CreateImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 resources.pong))
                    return false;
                VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                framebufferInfo.renderPass = offscreenRenderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.width = extent.width;
                framebufferInfo.height = extent.height;
                framebufferInfo.layers = 1;
                framebufferInfo.pAttachments = &resources.ping.view;
                if (!Check(vkCreateFramebuffer(createInfo.device, &framebufferInfo,
                                                createInfo.allocator, &resources.pingFramebuffer),
                           "Creating a Vulkan effect framebuffer failed."))
                    return false;
                framebufferInfo.pAttachments = &resources.pong.view;
                if (!Check(vkCreateFramebuffer(createInfo.device, &framebufferInfo,
                                                createInfo.allocator, &resources.pongFramebuffer),
                           "Creating a Vulkan effect framebuffer failed."))
                    return false;

                const VkDescriptorSetLayout layouts[] = {
                    descriptorSetLayout, descriptorSetLayout, descriptorSetLayout};
                VkDescriptorSetAllocateInfo allocateInfo{
                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                allocateInfo.descriptorPool = descriptorPool;
                allocateInfo.descriptorSetCount = 3;
                allocateInfo.pSetLayouts = layouts;
                if (!Check(vkAllocateDescriptorSets(createInfo.device, &allocateInfo,
                                                     resources.descriptorSets.data()),
                           "Allocating Vulkan effect descriptor sets failed."))
                    return false;
                VkImageView views[] = {resources.source.view, resources.ping.view, resources.pong.view};
                for (std::size_t index = 0; index < std::size(views); ++index)
                {
                    VkDescriptorImageInfo imageInfo{sampler, views[index],
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    write.dstSet = resources.descriptorSets[index];
                    write.dstBinding = 0;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    write.pImageInfo = &imageInfo;
                    vkUpdateDescriptorSets(createInfo.device, 1, &write, 0, nullptr);
                }
            }
            return true;
        }

        void ImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                          VkImageLayout newLayout, VkAccessFlags sourceAccess,
                          VkAccessFlags destinationAccess, VkPipelineStageFlags sourceStage,
                          VkPipelineStageFlags destinationStage)
        {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = sourceAccess;
            barrier.dstAccessMask = destinationAccess;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                                 nullptr, 1, &barrier);
        }

        void SetViewport(VkCommandBuffer commandBuffer)
        {
            const VkViewport viewport{0.0f, 0.0f, static_cast<float>(frame.extent.width),
                                      static_cast<float>(frame.extent.height), 0.0f, 1.0f};
            const VkRect2D scissor{{0, 0}, {frame.extent.width, frame.extent.height}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        }

        void DrawOffscreen(VkFramebuffer framebuffer, VkDescriptorSet sourceSet,
                           const EffectConstants& constants)
        {
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = offscreenRenderPass;
            begin.framebuffer = framebuffer;
            begin.renderArea.extent = {frame.extent.width, frame.extent.height};
            vkCmdBeginRenderPass(frame.commandBuffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
            SetViewport(frame.commandBuffer);
            vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipeline);
            vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &sourceSet, 0, nullptr);
            vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, kPushConstantSize, constants.values.data());
            vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(frame.commandBuffer);
        }

        bool Prepare(const EffectRendererFrameInfo& info)
        {
            frame = info;
            if (!EnsureResources(info.extent))
                return false;
            FrameResources& resources = frames[info.frameIndex % frames.size()];
            resources.prepared = false;
            VkImage captureImage = info.backdropImage ? info.backdropImage : info.targetImage;
            VkImageLayout captureLayout = info.backdropImage ? info.backdropLayout : info.targetLayout;
            if (captureImage == VK_NULL_HANDLE)
            {
                error = "Vulkan PrepareFrame requires a target or backdrop image.";
                return false;
            }
            ImageBarrier(info.commandBuffer, captureImage, captureLayout,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         captureLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                             ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            ImageBarrier(info.commandBuffer, resources.source.image,
                         resources.sourceInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                     : VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         resources.sourceInitialized ? VK_ACCESS_SHADER_READ_BIT : 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT,
                         resources.sourceInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                     : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkImageCopy copy{};
            copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.srcSubresource.layerCount = 1;
            copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.dstSubresource.layerCount = 1;
            copy.extent = {info.extent.width, info.extent.height, 1};
            vkCmdCopyImage(info.commandBuffer, captureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           resources.source.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            ImageBarrier(info.commandBuffer, resources.source.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            ImageBarrier(info.commandBuffer, captureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         captureLayout, VK_ACCESS_TRANSFER_READ_BIT,
                         captureLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                             ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            resources.sourceInitialized = true;
            DrawOffscreen(resources.pingFramebuffer, resources.descriptorSets[0],
                          MakeBlurEffectConstants(info.extent, {1.0f, 0.0f},
                                                  info.backdropBlurSigma));
            DrawOffscreen(resources.pongFramebuffer, resources.descriptorSets[1],
                          MakeBlurEffectConstants(info.extent, {0.0f, 1.0f},
                                                  info.backdropBlurSigma));
            resources.prepared = true;
            return true;
        }

        void DrawCallback(VkPipeline pipeline, VkDescriptorSet sourceSet,
                          const EffectConstants& constants)
        {
            SetViewport(frame.commandBuffer);
            vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &sourceSet, 0, nullptr);
            vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, kPushConstantSize, constants.values.data());
            vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);
        }

        void Render(const EffectRequest& request)
        {
            if (!frameActive || frames.empty())
                return;
            FrameResources& resources = frames[frame.frameIndex % frames.size()];
            if (!resources.prepared)
                return;
            switch (request.id)
            {
            case EffectId::BackdropBlur:
            {
                GlassEffectParams glass{};
                glass.bounds = request.blur.bounds;
                glass.opacity = request.blur.opacity;
                glass.rounding = request.blur.rounding;
                glass.tintStrength = 0.0f;
                glass.blurSigma = request.blur.sigma;
                EffectConstants constants = MakeGlassEffectConstants(glass, frame.extent,
                                                                      frame.timeSeconds);
                constants.values[31] = 1.0f;
                DrawCallback(materialPipeline, resources.descriptorSets[2], constants);
                break;
            }
            case EffectId::Glass:
                DrawCallback(materialPipeline, resources.descriptorSets[2],
                             MakeGlassEffectConstants(request.glass, frame.extent,
                                                      frame.timeSeconds));
                break;
            case EffectId::Shadow:
                DrawCallback(shapePipeline, resources.descriptorSets[2],
                             MakeShapeConstants(request.shadow.bounds, request.shadow.rounding,
                                                request.shadow.strength, request.shadow.sigma,
                                                request.shadow.color, request.shadow.offset));
                break;
            case EffectId::Glow:
                DrawCallback(shapePipeline, resources.descriptorSets[2],
                             MakeShapeConstants(request.glow.bounds, request.glow.rounding,
                                                request.glow.strength, request.glow.sigma,
                                                request.glow.color, {}));
                break;
            case EffectId::Bloom:
                DrawCallback(bloomPipeline, resources.descriptorSets[2],
                             MakeBloomEffectConstants(request.bloom, frame.extent));
                break;
            default:
                break;
            }
        }
    };

    std::unique_ptr<EffectRenderer> EffectRenderer::Create(
        const EffectRendererCreateInfo& createInfo) noexcept
    {
        try
        {
            auto impl = std::make_unique<Impl>(createInfo);
            if (!impl->Initialize())
                return nullptr;
            return std::unique_ptr<EffectRenderer>(new EffectRenderer(std::move(impl)));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    EffectRenderer::EffectRenderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
    EffectRenderer::~EffectRenderer() = default;
    EffectRenderer::EffectRenderer(EffectRenderer&&) noexcept = default;
    EffectRenderer& EffectRenderer::operator=(EffectRenderer&&) noexcept = default;

    bool EffectRenderer::PrepareFrame(const EffectRendererFrameInfo& frameInfo) noexcept
    {
        if (!impl_ || frameInfo.commandBuffer == VK_NULL_HANDLE || frameInfo.extent.width == 0 ||
            frameInfo.extent.height == 0)
            return false;
        try
        {
            return impl_->Prepare(frameInfo);
        }
        catch (...)
        {
            impl_->error = "Unexpected Vulkan effect preparation failure.";
            return false;
        }
    }

    bool EffectRenderer::BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept
    {
        if (!impl_ || frameInfo.commandBuffer == VK_NULL_HANDLE || impl_->frames.empty())
            return false;
        const auto& resources = impl_->frames[frameInfo.frameIndex % impl_->frames.size()];
        if (!resources.prepared || frameInfo.extent.width != impl_->resourceExtent.width ||
            frameInfo.extent.height != impl_->resourceExtent.height)
            return false;
        impl_->frame = frameInfo;
        impl_->frameActive = true;
        return true;
    }

    void EffectRenderer::EndFrame() noexcept
    {
        if (impl_)
        {
            impl_->frameActive = false;
            impl_->frame = {};
        }
    }

    const char* EffectRenderer::GetLastError() const noexcept
    {
        return impl_ ? impl_->error.c_str() : "Vulkan effect renderer is not initialized.";
    }

    EffectCapabilities EffectRenderer::GetCapabilities() const noexcept
    {
        return impl_ ? AllEffectCapabilities : EffectCapabilities::None;
    }

    void EffectRenderer::RenderEffect(const EffectRequest& request) noexcept
    {
        if (!impl_)
            return;
        try
        {
            impl_->Render(request);
        }
        catch (...)
        {
            impl_->error = "Unexpected Vulkan effect rendering failure.";
        }
    }
}
