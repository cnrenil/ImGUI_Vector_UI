/** @file ModelViewerDX12.h @brief Direct3D 12 texture bridge for ModelViewer. */
#pragma once

#include "ImGuiVectorUI/Backends/RendererDX12.h"
#include "ImGuiVectorUI/Optional/ModelViewer.h"

#include <d3d12.h>

#include <string>

namespace IVUI::DX12
{
    struct ModelViewerTextureBridgeCreateInfo
    {
        ID3D12Device* device = nullptr;
        ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
        AllocateSrv allocateSrv = nullptr;
        FreeSrv freeSrv = nullptr;
        void* userData = nullptr;
    };

    struct ModelViewerTextureBridgeCallbackContext
    {
        class ModelViewerTextureBridge* bridge = nullptr;
        ID3D12GraphicsCommandList* commandList = nullptr;
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
                           ID3D12GraphicsCommandList* commandList,
                           std::string* error = nullptr) noexcept;
        /** Call only after the host fence confirms prior uploads are complete. */
        void ResetAfterGpuIdle() noexcept;

        ImTextureID TextureId() const noexcept;
        D3D12_GPU_DESCRIPTOR_HANDLE GpuDescriptor() const noexcept { return gpuSrv_; }

        static ImTextureID UploadCallback(const ModelViewerOffscreenImage& image,
                                          void* userData) noexcept;

    private:
        bool CreateResources(int width, int height, std::string* error) noexcept;
        void ReleaseResources() noexcept;

        ModelViewerTextureBridgeCreateInfo info_{};
        ID3D12Resource* texture_ = nullptr;
        ID3D12Resource* upload_ = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv_{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv_{};
        D3D12_RESOURCE_STATES textureState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        int width_ = 0;
        int height_ = 0;
        bool descriptorAllocated_ = false;
        bool initialized_ = false;
    };
}
