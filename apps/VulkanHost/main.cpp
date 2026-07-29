// ImGuiVectorUI demonstration host using Win32 and Vulkan.

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"

#include "ImGuiVectorUI/ImGuiVectorUI.h"
#include "ImGuiVectorUI/Backends/EffectsVulkan.h"
#include "ImGuiVectorUI/Backends/PlatformWin32.h"
#include "ImGuiVectorUI/Backends/RendererVulkan.h"
#include "DemoMenu.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// These implementation helpers are linked by RendererVulkan but intentionally omitted
// from imgui_impl_vulkan.h because normal applications own their swapchain setup.
void ImGui_ImplVulkanH_CreateWindowSwapChain(
    VkPhysicalDevice physicalDevice, VkDevice device, ImGui_ImplVulkanH_Window* window,
    const VkAllocationCallbacks* allocator, int width, int height,
    std::uint32_t minImageCount, VkImageUsageFlags imageUsage);
void ImGui_ImplVulkanH_CreateWindowCommandBuffers(
    VkPhysicalDevice physicalDevice, VkDevice device, ImGui_ImplVulkanH_Window* window,
    std::uint32_t queueFamily, const VkAllocationCallbacks* allocator);

namespace
{
    constexpr std::uint32_t kApiVersion = VK_API_VERSION_1_0;
    constexpr std::uint32_t kMinImageCount = 2;

    VkInstance g_instance = VK_NULL_HANDLE;
    VkSurfaceKHR g_surface = VK_NULL_HANDLE;
    VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
    VkDevice g_device = VK_NULL_HANDLE;
    std::uint32_t g_queueFamily = UINT32_MAX;
    VkQueue g_queue = VK_NULL_HANDLE;
    VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Window g_windowData;
    bool g_swapChainRebuild = false;
    UINT g_resizeWidth = 0;
    UINT g_resizeHeight = 0;
    VkResult g_backendResult = VK_SUCCESS;
    std::vector<bool> g_imageInitialized;
    std::unique_ptr<IVUI::UI> g_ui;
    HWND g_hwnd = nullptr;

    void CheckVkResult(VkResult result)
    {
        if (result < 0)
        {
            g_backendResult = result;
            std::abort();
        }
    }

    bool Check(VkResult result, const char* operation, std::string& error)
    {
        if (result == VK_SUCCESS)
            return true;
        error = operation + std::string(" failed (VkResult ") +
                std::to_string(static_cast<int>(result)) + ").";
        return false;
    }

    bool HasDeviceExtension(VkPhysicalDevice device, const char* requested)
    {
        std::uint32_t count = 0;
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS)
            return false;
        std::vector<VkExtensionProperties> properties(count);
        if (vkEnumerateDeviceExtensionProperties(
                device, nullptr, &count, properties.data()) != VK_SUCCESS)
            return false;
        for (const VkExtensionProperties& property : properties)
            if (std::strcmp(property.extensionName, requested) == 0)
                return true;
        return false;
    }

    bool ValidateWindowResources(std::string& error)
    {
        if (g_windowData.Swapchain == VK_NULL_HANDLE ||
            g_windowData.RenderPass == VK_NULL_HANDLE ||
            g_windowData.ImageCount < kMinImageCount ||
            g_windowData.Frames.Size != static_cast<int>(g_windowData.ImageCount) ||
            g_windowData.SemaphoreCount == 0 ||
            g_windowData.FrameSemaphores.Size !=
                static_cast<int>(g_windowData.SemaphoreCount))
        {
            error = "Creating the Vulkan swapchain resources failed.";
            return false;
        }
        for (const ImGui_ImplVulkanH_Frame& frame : g_windowData.Frames)
        {
            if (frame.Backbuffer == VK_NULL_HANDLE ||
                frame.BackbufferView == VK_NULL_HANDLE ||
                frame.Framebuffer == VK_NULL_HANDLE ||
                frame.CommandPool == VK_NULL_HANDLE ||
                frame.CommandBuffer == VK_NULL_HANDLE || frame.Fence == VK_NULL_HANDLE)
            {
                error = "Creating a Vulkan swapchain frame resource failed.";
                return false;
            }
        }
        for (const ImGui_ImplVulkanH_FrameSemaphores& semaphores :
             g_windowData.FrameSemaphores)
        {
            if (semaphores.ImageAcquiredSemaphore == VK_NULL_HANDLE ||
                semaphores.RenderCompleteSemaphore == VK_NULL_HANDLE)
            {
                error = "Creating the Vulkan frame semaphores failed.";
                return false;
            }
        }
        return true;
    }

    void CreateOrResizeWindow(int width, int height)
    {
        ImGui_ImplVulkanH_CreateWindowSwapChain(
            g_physicalDevice, g_device, &g_windowData, nullptr, width, height,
            kMinImageCount,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        ImGui_ImplVulkanH_CreateWindowCommandBuffers(
            g_physicalDevice, g_device, &g_windowData, g_queueFamily, nullptr);
    }

    bool SetupVulkan(HWND hwnd, std::string& error)
    {
        const char* instanceExtensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        applicationInfo.pApplicationName = "ImGuiVectorUI Vulkan Demo";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "ImGuiVectorUI";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = kApiVersion;
        VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instanceInfo.pApplicationInfo = &applicationInfo;
        instanceInfo.enabledExtensionCount = static_cast<std::uint32_t>(std::size(instanceExtensions));
        instanceInfo.ppEnabledExtensionNames = instanceExtensions;
        if (!Check(vkCreateInstance(&instanceInfo, nullptr, &g_instance),
                   "Creating the Vulkan instance", error))
            return false;

        VkWin32SurfaceCreateInfoKHR surfaceInfo{
            VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        surfaceInfo.hinstance = ::GetModuleHandleW(nullptr);
        surfaceInfo.hwnd = hwnd;
        if (!Check(vkCreateWin32SurfaceKHR(g_instance, &surfaceInfo, nullptr, &g_surface),
                   "Creating the Win32 Vulkan surface", error))
            return false;

        std::uint32_t physicalDeviceCount = 0;
        if (!Check(vkEnumeratePhysicalDevices(g_instance, &physicalDeviceCount, nullptr),
                   "Enumerating Vulkan physical devices", error) || physicalDeviceCount == 0)
        {
            if (error.empty())
                error = "No Vulkan physical device is available.";
            return false;
        }
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        if (!Check(vkEnumeratePhysicalDevices(
                       g_instance, &physicalDeviceCount, physicalDevices.data()),
                   "Enumerating Vulkan physical devices", error))
            return false;

        for (VkPhysicalDevice device : physicalDevices)
        {
            if (!HasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                continue;
            std::uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                device, &queueFamilyCount, queueFamilies.data());
            for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
            {
                VkBool32 supportsPresent = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    device, index, g_surface, &supportsPresent);
                if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                    supportsPresent == VK_TRUE)
                {
                    g_physicalDevice = device;
                    g_queueFamily = index;
                    break;
                }
            }
            if (g_physicalDevice != VK_NULL_HANDLE)
                break;
        }
        if (g_physicalDevice == VK_NULL_HANDLE)
        {
            error = "No Vulkan device has a queue that supports graphics and Win32 presentation.";
            return false;
        }

        VkSurfaceCapabilitiesKHR capabilities{};
        if (!Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                       g_physicalDevice, g_surface, &capabilities),
                   "Querying Vulkan surface capabilities", error))
            return false;
        constexpr VkImageUsageFlags effectImageUsage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ((capabilities.supportedUsageFlags & effectImageUsage) != effectImageUsage)
        {
            error = "The Vulkan surface does not support transfer-source and transfer-destination swapchain images.";
            return false;
        }

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = g_queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;
        if (!Check(vkCreateDevice(g_physicalDevice, &deviceInfo, nullptr, &g_device),
                   "Creating the Vulkan device", error))
            return false;
        vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_queue);

        const VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 128},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 384;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;
        if (!Check(vkCreateDescriptorPool(
                       g_device, &poolInfo, nullptr, &g_descriptorPool),
                   "Creating the Vulkan descriptor pool", error))
            return false;

        const VkFormat requestedFormats[] = {
            VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
        g_windowData.Surface = g_surface;
        g_windowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
            g_physicalDevice, g_surface, requestedFormats, std::size(requestedFormats),
            VK_COLORSPACE_SRGB_NONLINEAR_KHR);
        const VkPresentModeKHR requestedPresentModes[] = {VK_PRESENT_MODE_FIFO_KHR};
        g_windowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
            g_physicalDevice, g_surface, requestedPresentModes,
            std::size(requestedPresentModes));

        RECT clientRect{};
        ::GetClientRect(hwnd, &clientRect);
        CreateOrResizeWindow(
            clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
        if (!ValidateWindowResources(error))
            return false;
        g_imageInitialized.assign(g_windowData.ImageCount, false);
        return true;
    }

    void CleanupVulkan()
    {
        if (g_device)
            vkDeviceWaitIdle(g_device);
        g_ui.reset();
        if (g_device && g_windowData.Swapchain)
            ImGui_ImplVulkanH_DestroyWindow(
                g_instance, g_device, &g_windowData, nullptr);
        if (g_device && g_descriptorPool)
            vkDestroyDescriptorPool(g_device, g_descriptorPool, nullptr);
        if (g_device)
            vkDestroyDevice(g_device, nullptr);
        if (g_instance && g_surface)
            vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        if (g_instance)
            vkDestroyInstance(g_instance, nullptr);
        g_descriptorPool = VK_NULL_HANDLE;
        g_queue = VK_NULL_HANDLE;
        g_device = VK_NULL_HANDLE;
        g_physicalDevice = VK_NULL_HANDLE;
        g_surface = VK_NULL_HANDLE;
        g_instance = VK_NULL_HANDLE;
    }

    std::string ExecutableDirectory()
    {
        char path[MAX_PATH]{};
        ::GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string result(path);
        return result.substr(0, result.find_last_of("\\/") + 1);
    }

    void ConfigureFonts(const std::string& unifontPath, bool haveUnifont,
                        const std::shared_ptr<bool>& unifontLoaded, ImGuiIO& io,
                        ImFont*& regular, ImFont*& title)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.WindowPadding = {};
        style.Colors[ImGuiCol_WindowBg] = style.Colors[ImGuiCol_ChildBg] =
            style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);

        auto mergeUnifont = [&](float size) {
            if (!haveUnifont)
                return false;
            ImFontConfig config;
            config.MergeMode = true;
            config.PixelSnapH = true;
            return io.Fonts->AddFontFromFileTTF(
                       unifontPath.c_str(), size, &config,
                       io.Fonts->GetGlyphRangesChineseFull()) != nullptr;
        };
        ImFontConfig base;
        base.OversampleH = 3;
        base.PixelSnapH = true;
        regular = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &base);
        if (!regular)
            regular = io.Fonts->AddFontDefault();
        const bool regularMerged = mergeUnifont(18.0f);
        title = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f, &base);
        if (!title)
            title = regular;
        const bool titleMerged = mergeUnifont(20.0f);
        *unifontLoaded = regularMerged && titleMerged;
    }

    void BeginRenderPass(void* userData, VkCommandBuffer commandBuffer)
    {
        auto* frame = static_cast<ImGui_ImplVulkanH_Frame*>(userData);
        VkRenderPassBeginInfo beginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        beginInfo.renderPass = g_windowData.RenderPass;
        beginInfo.framebuffer = frame->Framebuffer;
        beginInfo.renderArea.extent = {
            static_cast<std::uint32_t>(g_windowData.Width),
            static_cast<std::uint32_t>(g_windowData.Height)};
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = &g_windowData.ClearValue;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void EndRenderPass(void*, VkCommandBuffer commandBuffer)
    {
        vkCmdEndRenderPass(commandBuffer);
    }

    bool RebuildSwapChain(std::string& error)
    {
        if (g_resizeWidth == 0 || g_resizeHeight == 0)
        {
            RECT clientRect{};
            ::GetClientRect(g_hwnd, &clientRect);
            g_resizeWidth = static_cast<UINT>(clientRect.right - clientRect.left);
            g_resizeHeight = static_cast<UINT>(clientRect.bottom - clientRect.top);
            if (g_resizeWidth == 0 || g_resizeHeight == 0)
                return true;
        }
        if (!Check(vkDeviceWaitIdle(g_device), "Waiting for the Vulkan device", error))
            return false;
        const std::uint32_t previousImageCount = g_windowData.ImageCount;
        CreateOrResizeWindow(
            static_cast<int>(g_resizeWidth), static_cast<int>(g_resizeHeight));
        if (!ValidateWindowResources(error))
            return false;
        if (g_windowData.ImageCount != previousImageCount)
        {
            error = "The Vulkan swapchain image count changed; recreate the UI renderer for this surface.";
            return false;
        }
        g_windowData.FrameIndex = 0;
        g_windowData.SemaphoreIndex = 0;
        g_imageInitialized.assign(g_windowData.ImageCount, false);
        g_resizeWidth = g_resizeHeight = 0;
        g_swapChainRebuild = false;
        return g_backendResult == VK_SUCCESS;
    }

    bool RenderFrame(std::string& error)
    {
        VkSemaphore imageAcquired = g_windowData
            .FrameSemaphores[g_windowData.SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore renderComplete = g_windowData
            .FrameSemaphores[g_windowData.SemaphoreIndex].RenderCompleteSemaphore;
        VkResult result = vkAcquireNextImageKHR(
            g_device, g_windowData.Swapchain, UINT64_MAX, imageAcquired,
            VK_NULL_HANDLE, &g_windowData.FrameIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            g_swapChainRebuild = true;
            g_ui->EndFrameWithoutRender();
            return true;
        }
        if (result == VK_SUBOPTIMAL_KHR)
            g_swapChainRebuild = true;
        else if (!Check(result, "Acquiring a Vulkan swapchain image", error))
            return false;

        ImGui_ImplVulkanH_Frame& frame = g_windowData.Frames[g_windowData.FrameIndex];
        if (!Check(vkWaitForFences(
                       g_device, 1, &frame.Fence, VK_TRUE, UINT64_MAX),
                   "Waiting for the Vulkan frame fence", error) ||
            !Check(vkResetFences(g_device, 1, &frame.Fence),
                   "Resetting the Vulkan frame fence", error) ||
            !Check(vkResetCommandPool(g_device, frame.CommandPool, 0),
                   "Resetting the Vulkan command pool", error))
            return false;

        VkCommandBufferBeginInfo beginInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!Check(vkBeginCommandBuffer(frame.CommandBuffer, &beginInfo),
                   "Beginning the Vulkan command buffer", error))
            return false;

        if (!g_imageInitialized[g_windowData.FrameIndex])
        {
            VkImageMemoryBarrier initializeBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            initializeBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            initializeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            initializeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            initializeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            initializeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            initializeBarrier.image = frame.Backbuffer;
            initializeBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            initializeBarrier.subresourceRange.levelCount = 1;
            initializeBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                frame.CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                1, &initializeBarrier);
            const VkClearColorValue clearColor{{0.008f, 0.012f, 0.018f, 1.0f}};
            const VkImageSubresourceRange imageRange{
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdClearColorImage(
                frame.CommandBuffer, frame.Backbuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &imageRange);
            initializeBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            initializeBarrier.dstAccessMask = 0;
            initializeBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            initializeBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier(
                frame.CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                1, &initializeBarrier);
            g_imageInitialized[g_windowData.FrameIndex] = true;
        }

        IVUI::Vulkan::RenderFrame renderFrame;
        renderFrame.effects.commandBuffer = frame.CommandBuffer;
        renderFrame.effects.targetImage = frame.Backbuffer;
        renderFrame.effects.targetView = frame.BackbufferView;
        renderFrame.effects.targetLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        renderFrame.effects.extent = {
            static_cast<std::uint32_t>(g_windowData.Width),
            static_cast<std::uint32_t>(g_windowData.Height)};
        renderFrame.effects.frameIndex = g_windowData.FrameIndex;
        renderFrame.effects.timeSeconds = static_cast<float>(ImGui::GetTime());
        const IVUI::Theme& theme = g_ui->theme().Current();
        const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(theme.Accent);
        renderFrame.effects.backdropBlurSigma = theme.BackdropBlurSigma;
        renderFrame.effects.renderBackground = true;
        renderFrame.effects.background.accent = {accent.x, accent.y, accent.z, accent.w};
        renderFrame.effects.background.gridSize = 48.0f * theme.Scale;
        renderFrame.effects.background.animationSpeed = 1.15f;
        renderFrame.effects.background.intensity = 1.3f;
        renderFrame.effects.allowTargetCapture = true;
        renderFrame.effects.renderBloom = theme.PanelGlow > 0.001f;
        renderFrame.effects.bloom.bounds = {
            {0.0f, 0.0f},
            {static_cast<float>(g_windowData.Width),
             static_cast<float>(g_windowData.Height)}};
        renderFrame.effects.bloom.sigma = theme.BloomSigma;
        renderFrame.effects.bloom.strength =
            std::clamp(theme.PanelGlow * 1.35f, 0.0f, 1.5f);
        renderFrame.effects.bloom.brightnessThreshold = theme.BloomThreshold;
        renderFrame.effects.bloom.saturationThreshold = theme.BloomSaturation;
        renderFrame.beginRenderPass = BeginRenderPass;
        renderFrame.endRenderPass = EndRenderPass;
        renderFrame.userData = &frame;
        if (!g_ui->Render(renderFrame, &error))
            return false;

        if (!Check(vkEndCommandBuffer(frame.CommandBuffer),
                   "Ending the Vulkan command buffer", error))
            return false;
        const VkPipelineStageFlags waitStage =
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAcquired;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.CommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderComplete;
        if (!Check(vkQueueSubmit(g_queue, 1, &submitInfo, frame.Fence),
                   "Submitting the Vulkan frame", error))
            return false;

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderComplete;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &g_windowData.Swapchain;
        presentInfo.pImageIndices = &g_windowData.FrameIndex;
        result = vkQueuePresentKHR(g_queue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            g_swapChainRebuild = true;
        else if (!Check(result, "Presenting the Vulkan frame", error))
            return false;
        g_windowData.SemaphoreIndex =
            (g_windowData.SemaphoreIndex + 1) % g_windowData.SemaphoreCount;
        return true;
    }

    LRESULT WINAPI WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (g_ui)
        {
            const IVUI::Win32::Event event{hwnd, message, wParam, lParam};
            if (g_ui->ProcessEvent(IVUI::Win32::MakeEvent(event)))
                return true;
        }
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (g_device != VK_NULL_HANDLE && wParam != SIZE_MINIMIZED)
            {
                g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
                g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
                g_swapChainRebuild = true;
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW windowClass = {
        sizeof(windowClass), CS_CLASSDC, WndProc, 0L, 0L, instance, nullptr, nullptr,
        nullptr, nullptr, L"ImGuiVectorUIDemoVulkan", nullptr};
    ::RegisterClassExW(&windowClass);
    HWND hwnd = ::CreateWindowExW(
        0, windowClass.lpszClassName, L"ImGUI_Vector_UI - Vulkan",
        WS_OVERLAPPEDWINDOW, 100, 100, 1600, 900, nullptr, nullptr,
        windowClass.hInstance, nullptr);
    g_hwnd = hwnd;

    const float initialDpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    int width = static_cast<int>(1600.0f * initialDpi);
    int height = static_cast<int>(900.0f * initialDpi);
    const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
    width = (width > static_cast<int>(screenWidth * 0.92f))
        ? static_cast<int>(screenWidth * 0.92f) : width;
    height = (height > static_cast<int>(screenHeight * 0.92f))
        ? static_cast<int>(screenHeight * 0.92f) : height;
    ::SetWindowPos(hwnd, nullptr, (screenWidth - width) / 2, (screenHeight - height) / 2,
                   width, height, SWP_NOZORDER | SWP_NOACTIVATE);

    std::string error;
    if (!SetupVulkan(hwnd, error))
    {
        ::MessageBoxA(hwnd, error.c_str(), "ImGuiVectorUI Vulkan", MB_OK | MB_ICONERROR);
        CleanupVulkan();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);

    IMGUI_CHECKVERSION();
    const std::string executableDirectory = ExecutableDirectory();
    auto shaderLibrary = IVUI::Vulkan::EffectShaderLibrary::Load(
        (executableDirectory + "shaders").c_str(), &error);
    if (!shaderLibrary)
    {
        ::MessageBoxA(hwnd, error.c_str(), "ImGuiVectorUI Vulkan shaders",
                      MB_OK | MB_ICONERROR);
        CleanupVulkan();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    auto effects = IVUI::Vulkan::EffectRenderer::Create({
        g_physicalDevice, g_device, g_windowData.SurfaceFormat.format,
        g_windowData.RenderPass, 0, g_windowData.ImageCount, nullptr,
        shaderLibrary->Get()});
    if (!effects)
    {
        ::MessageBoxA(hwnd, "Vulkan effects initialization failed.",
                      "ImGuiVectorUI Vulkan", MB_OK | MB_ICONERROR);
        CleanupVulkan();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    const std::string unifontPath = executableDirectory + "unifont.otf";
    const bool haveUnifont =
        ::GetFileAttributesA(unifontPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    const auto unifontLoaded = std::make_shared<bool>(false);
    IVUI::UIConfig config;
    config.platform = std::make_unique<IVUI::Win32::PlatformBackend>(hwnd);
    config.renderer = std::make_unique<IVUI::Vulkan::RendererBackend>(
        IVUI::Vulkan::RendererCreateInfo{
            kApiVersion, g_instance, g_physicalDevice, g_device, g_queueFamily,
            g_queue, g_descriptorPool, 0, kMinImageCount, g_windowData.ImageCount,
            VK_NULL_HANDLE, g_windowData.RenderPass, 0, VK_SAMPLE_COUNT_1_BIT,
            nullptr, CheckVkResult});
    config.effects = std::move(effects);
    config.contentScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    config.configureFonts = [unifontPath, haveUnifont, unifontLoaded](
                                ImGuiIO& io, ImFont*& regular, ImFont*& title) {
        ConfigureFonts(unifontPath, haveUnifont, unifontLoaded, io, regular, title);
    };
    g_ui = IVUI::UI::Create(std::move(config), &error);
    if (!g_ui || !*unifontLoaded || g_backendResult != VK_SUCCESS)
    {
        const std::string message = !g_ui
            ? (error.empty() ? "IVUI initialization failed." : error)
            : (g_backendResult != VK_SUCCESS
                   ? "The Vulkan renderer backend reported an initialization error."
                   : (haveUnifont
                          ? "Unifont could not be parsed or merged into the font atlas."
                          : "unifont.otf was not found beside ImGuiVectorUIDemoVulkan.exe."));
        ::MessageBoxA(hwnd, message.c_str(), "ImGuiVectorUI Vulkan initialization",
                      MB_OK | MB_ICONERROR);
        CleanupVulkan();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    g_windowData.ClearValue.color.float32[0] = 0.008f;
    g_windowData.ClearValue.color.float32[1] = 0.012f;
    g_windowData.ClearValue.color.float32[2] = 0.018f;
    g_windowData.ClearValue.color.float32[3] = 1.0f;

    bool showMenu = true;
    bool done = false;
    while (!done)
    {
        MSG message;
        while (::PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessage(&message);
            if (message.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if (::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }
        if (g_swapChainRebuild && !RebuildSwapChain(error))
        {
            ::MessageBoxA(hwnd, error.c_str(), "ImGuiVectorUI Vulkan resize",
                          MB_OK | MB_ICONERROR);
            break;
        }

        const bool togglePressed = ((::GetAsyncKeyState(VK_INSERT) & 1) != 0) ||
                                   ((::GetAsyncKeyState(VK_F1) & 1) != 0);
        if (togglePressed && !g_ui->WantCaptureHotkey() && !g_ui->WantTextInput())
            showMenu = !showMenu;
        if (!g_ui->BeginFrame({ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd)}, &error))
        {
            ::MessageBoxA(hwnd, error.c_str(), "ImGuiVectorUI Vulkan",
                          MB_OK | MB_ICONERROR);
            break;
        }
        Demo::RenderMenu(*g_ui, &showMenu);
        if (!RenderFrame(error))
        {
            ::MessageBoxA(hwnd, error.c_str(), "ImGuiVectorUI Vulkan render error",
                          MB_OK | MB_ICONERROR);
            break;
        }
    }

    CleanupVulkan();
    shaderLibrary.reset();
    ::DestroyWindow(hwnd);
    g_hwnd = nullptr;
    ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return 0;
}
