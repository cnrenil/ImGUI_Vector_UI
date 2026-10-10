#include "ImGuiVectorUI/Backends/ModelViewerDX11.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace IVUI::DX11
{
    namespace
    {
        void SetError(std::string* error, const char* message)
        {
            if (error)
                *error = message ? message : "ModelViewer DX11 texture bridge failed.";
        }

        ImTextureID ToTextureId(ID3D11ShaderResourceView* view) noexcept
        {
            return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(view));
        }
    }

    ModelViewerTextureBridge::ModelViewerTextureBridge(
        ModelViewerTextureBridgeCreateInfo info) noexcept : info_(info) {}

    ModelViewerTextureBridge::~ModelViewerTextureBridge()
    {
        Reset();
    }

    bool ModelViewerTextureBridge::Initialize(std::string* error) noexcept
    {
        if (!info_.device || !info_.deviceContext)
        {
            SetError(error, "ModelViewer DX11 requires a device and immediate context.");
            return false;
        }
        initialized_ = true;
        if (error)
            error->clear();
        return true;
    }

    bool ModelViewerTextureBridge::Resize(int width, int height, std::string* error) noexcept
    {
        if (width <= 0 || height <= 0 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
            height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        {
            SetError(error, "ModelViewer DX11 image dimensions are invalid.");
            return false;
        }

        D3D11_TEXTURE2D_DESC description{};
        description.Width = static_cast<UINT>(width);
        description.Height = static_cast<UINT>(height);
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        ID3D11Texture2D* texture = nullptr;
        HRESULT result = info_.device->CreateTexture2D(&description, nullptr, &texture);
        if (FAILED(result))
        {
            SetError(error, "Creating the ModelViewer DX11 texture failed.");
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
        viewDescription.Format = description.Format;
        viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDescription.Texture2D.MipLevels = 1;
        ID3D11ShaderResourceView* view = nullptr;
        result = info_.device->CreateShaderResourceView(texture, &viewDescription, &view);
        if (FAILED(result))
        {
            texture->Release();
            SetError(error, "Creating the ModelViewer DX11 shader view failed.");
            return false;
        }

        if (view_)
            view_->Release();
        if (texture_)
            texture_->Release();
        texture_ = texture;
        view_ = view;
        width_ = width;
        height_ = height;
        return true;
    }

    ImTextureID ModelViewerTextureBridge::Upload(const ModelViewerOffscreenImage& image,
                                                 std::string* error) noexcept
    {
        if (!initialized_ && !Initialize(error))
            return ImTextureID_Invalid;
        if (!image.Valid())
        {
            SetError(error, "ModelViewer DX11 received an invalid RGBA image.");
            return ImTextureID_Invalid;
        }
        if ((width_ != image.width || height_ != image.height) &&
            !Resize(image.width, image.height, error))
            return ImTextureID_Invalid;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(info_.deviceContext->Map(texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            SetError(error, "Mapping the ModelViewer DX11 texture failed.");
            return ImTextureID_Invalid;
        }
        const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4u;
        for (int row = 0; row < image.height; ++row)
            std::memcpy(static_cast<std::uint8_t*>(mapped.pData) +
                            static_cast<std::size_t>(row) * mapped.RowPitch,
                        image.pixels.data() + static_cast<std::size_t>(row) * image.strideBytes,
                        rowBytes);
        info_.deviceContext->Unmap(texture_, 0);
        if (error)
            error->clear();
        return TextureId();
    }

    void ModelViewerTextureBridge::Reset() noexcept
    {
        if (view_)
            view_->Release();
        if (texture_)
            texture_->Release();
        view_ = nullptr;
        texture_ = nullptr;
        width_ = height_ = 0;
        initialized_ = false;
    }

    ImTextureID ModelViewerTextureBridge::TextureId() const noexcept
    {
        return ToTextureId(view_);
    }

    ImTextureID ModelViewerTextureBridge::UploadCallback(
        const ModelViewerOffscreenImage& image, void* userData) noexcept
    {
        auto* bridge = static_cast<ModelViewerTextureBridge*>(userData);
        return bridge ? bridge->Upload(image) : ImTextureID_Invalid;
    }
}
