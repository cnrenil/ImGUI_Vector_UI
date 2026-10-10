#include "ImGuiVectorUI/Backends/ModelViewerDX12.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace IVUI::DX12
{
    namespace
    {
        void SetError(std::string* error, const char* message)
        {
            if (error)
                *error = message ? message : "ModelViewer DX12 texture bridge failed.";
        }

        ImTextureID ToTextureId(D3D12_GPU_DESCRIPTOR_HANDLE handle) noexcept
        {
            return static_cast<ImTextureID>(handle.ptr);
        }

        UINT64 Align(UINT64 value, UINT64 alignment) noexcept
        {
            return (value + alignment - 1u) & ~(alignment - 1u);
        }
    }

    ModelViewerTextureBridge::ModelViewerTextureBridge(
        ModelViewerTextureBridgeCreateInfo info) noexcept : info_(info) {}

    ModelViewerTextureBridge::~ModelViewerTextureBridge()
    {
        ResetAfterGpuIdle();
    }

    bool ModelViewerTextureBridge::Initialize(std::string* error) noexcept
    {
        if (!info_.device || !info_.srvDescriptorHeap || !info_.allocateSrv ||
            !info_.freeSrv)
        {
            SetError(error, "ModelViewer DX12 requires a device and SRV allocator.");
            return false;
        }
        initialized_ = true;
        if (error)
            error->clear();
        return true;
    }

    bool ModelViewerTextureBridge::CreateResources(int width, int height,
                                                   std::string* error) noexcept
    {
        if (width <= 0 || height <= 0)
        {
            SetError(error, "ModelViewer DX12 image dimensions are invalid.");
            return false;
        }

        info_.allocateSrv(info_.userData, &cpuSrv_, &gpuSrv_);
        descriptorAllocated_ = true;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CreationNodeMask = 1;
        defaultHeap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC textureDescription{};
        textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDescription.Width = static_cast<UINT64>(width);
        textureDescription.Height = static_cast<UINT>(height);
        textureDescription.DepthOrArraySize = 1;
        textureDescription.MipLevels = 1;
        textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
        HRESULT result = info_.device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture_));
        if (FAILED(result))
        {
            SetError(error, "Creating the ModelViewer DX12 texture failed.");
            ReleaseResources();
            return false;
        }

        UINT numRows = 0;
        UINT64 rowSize = 0;
        UINT64 uploadSize = 0;
        info_.device->GetCopyableFootprints(&textureDescription, 0, 1, 0, nullptr,
                                            &numRows, &rowSize, &uploadSize);
        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeap.CreationNodeMask = 1;
        uploadHeap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC uploadDescription{};
        uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDescription.Width = uploadSize;
        uploadDescription.Height = 1;
        uploadDescription.DepthOrArraySize = 1;
        uploadDescription.MipLevels = 1;
        uploadDescription.Format = DXGI_FORMAT_UNKNOWN;
        uploadDescription.SampleDesc.Count = 1;
        uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        result = info_.device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_));
        if (FAILED(result))
        {
            SetError(error, "Creating the ModelViewer DX12 upload buffer failed.");
            ReleaseResources();
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDescription{};
        viewDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        viewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        viewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        viewDescription.Texture2D.MipLevels = 1;
        info_.device->CreateShaderResourceView(texture_, &viewDescription, cpuSrv_);
        width_ = width;
        height_ = height;
        textureState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        return true;
    }

    ImTextureID ModelViewerTextureBridge::Upload(const ModelViewerOffscreenImage& image,
                                                 ID3D12GraphicsCommandList* commandList,
                                                 std::string* error) noexcept
    {
        if (!initialized_ && !Initialize(error))
            return ImTextureID_Invalid;
        if (!commandList || !image.Valid())
        {
            SetError(error, "ModelViewer DX12 requires a command list and valid RGBA image.");
            return ImTextureID_Invalid;
        }
        if (texture_ && (width_ != image.width || height_ != image.height))
        {
            SetError(error, "ModelViewer DX12 size changes require ResetAfterGpuIdle first.");
            return ImTextureID_Invalid;
        }
        if (!texture_ && !CreateResources(image.width, image.height, error))
            return ImTextureID_Invalid;

        D3D12_RESOURCE_DESC description = texture_->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 rowSize = 0;
        UINT64 totalBytes = 0;
        info_.device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, &rows,
                                            &rowSize, &totalBytes);
        void* mapped = nullptr;
        D3D12_RANGE range{0, 0};
        if (FAILED(upload_->Map(0, &range, &mapped)))
        {
            SetError(error, "Mapping the ModelViewer DX12 upload buffer failed.");
            return ImTextureID_Invalid;
        }
        auto* destination = static_cast<std::uint8_t*>(mapped) + footprint.Offset;
        const std::size_t sourceRowBytes = static_cast<std::size_t>(image.width) * 4u;
        for (int row = 0; row < image.height; ++row)
            std::memcpy(destination + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                        image.pixels.data() + static_cast<std::size_t>(row) * image.strideBytes,
                        sourceRowBytes);
        upload_->Unmap(0, nullptr);

        if (textureState_ != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = texture_;
            barrier.Transition.StateBefore = textureState_;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
        }

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload_;
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
        destinationLocation.pResource = texture_;
        destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destinationLocation.SubresourceIndex = 0;
        commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &source, nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture_;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        textureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (error)
            error->clear();
        return TextureId();
    }

    void ModelViewerTextureBridge::ReleaseResources() noexcept
    {
        if (upload_)
            upload_->Release();
        if (texture_)
            texture_->Release();
        upload_ = nullptr;
        texture_ = nullptr;
        if (descriptorAllocated_ && info_.freeSrv)
            info_.freeSrv(info_.userData, cpuSrv_, gpuSrv_);
        descriptorAllocated_ = false;
        cpuSrv_ = {};
        gpuSrv_ = {};
        width_ = height_ = 0;
        textureState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    void ModelViewerTextureBridge::ResetAfterGpuIdle() noexcept
    {
        ReleaseResources();
        initialized_ = false;
    }

    ImTextureID ModelViewerTextureBridge::TextureId() const noexcept
    {
        return ToTextureId(gpuSrv_);
    }

    ImTextureID ModelViewerTextureBridge::UploadCallback(
        const ModelViewerOffscreenImage& image, void* userData) noexcept
    {
        auto* callback = static_cast<ModelViewerTextureBridgeCallbackContext*>(userData);
        return callback && callback->bridge
            ? callback->bridge->Upload(image, callback->commandList)
            : ImTextureID_Invalid;
    }
}
