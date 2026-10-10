/** @file ModelViewerDX11.h @brief Direct3D 11 texture bridge for ModelViewer. */
#pragma once

#include "ImGuiVectorUI/Optional/ModelViewer.h"

#include <d3d11.h>

#include <string>

namespace IVUI::DX11
{
    struct ModelViewerTextureBridgeCreateInfo
    {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* deviceContext = nullptr;
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
                           std::string* error = nullptr) noexcept;
        void Reset() noexcept;

        ImTextureID TextureId() const noexcept;
        ID3D11ShaderResourceView* ShaderResourceView() const noexcept { return view_; }

        static ImTextureID UploadCallback(const ModelViewerOffscreenImage& image,
                                          void* userData) noexcept;

    private:
        bool Resize(int width, int height, std::string* error) noexcept;

        ModelViewerTextureBridgeCreateInfo info_{};
        ID3D11Texture2D* texture_ = nullptr;
        ID3D11ShaderResourceView* view_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        bool initialized_ = false;
    };
}
