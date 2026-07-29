/** @file RendererDX11.h @brief Dear ImGui Direct3D 11 renderer adapter. */
#pragma once

#include "ImGuiVectorUI/UI.h"
#include "ImGuiVectorUI/Backends/EffectsDX11.h"

namespace IVUI::DX11
{
    struct RendererCreateInfo
    {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* deviceContext = nullptr;
    };

    struct RenderFrame final : IVUI::RenderFrame
    {
        EffectRendererFrameInfo effects{};
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
