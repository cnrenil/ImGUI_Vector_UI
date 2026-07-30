/** @file EffectsDX12.h
 *  @brief Direct3D 12 implementation of the IVUI effect renderer.
 */
#pragma once

#include "ImGuiVectorUI/Drawing/Effects.h"

#include <d3d12.h>
#include <dxgiformat.h>

#include <memory>
#include <string>

namespace IVUI::DX12
{
    using BackgroundShaderParams = IVUI::BackgroundShaderParams;

    struct EffectRendererCreateInfo
    {
        ID3D12Device* device = nullptr;
        DXGI_FORMAT targetFormat = DXGI_FORMAT_UNKNOWN;
        UINT nodeMask = 0;
    };

    struct EffectRendererFrameInfo
    {
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12Resource* target = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE targetRtv{};
        D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        EffectExtent extent{};
        float timeSeconds = 0.0f;
        ID3D12Resource* backdrop = nullptr;
        D3D12_RESOURCE_STATES backdropState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        ID3D12DescriptorHeap* restoreSrvDescriptorHeap = nullptr;
        bool renderBackground = false;
        BackgroundShaderParams background{};
        bool renderBloom = false;
        BloomEffectParams bloom{};
    };

    class EffectRenderer final : public IEffectRenderer
    {
    public:
        static std::unique_ptr<EffectRenderer> Create(
            const EffectRendererCreateInfo& createInfo,
            std::string* error = nullptr) noexcept;

        ~EffectRenderer() override;
        EffectRenderer(EffectRenderer&&) noexcept;
        EffectRenderer& operator=(EffectRenderer&&) noexcept;
        EffectRenderer(const EffectRenderer&) = delete;
        EffectRenderer& operator=(const EffectRenderer&) = delete;

        bool BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept;
        void EndFrame() noexcept;
        const char* GetLastError() const noexcept;

        EffectCapabilities GetCapabilities() const noexcept override;
        void RenderEffect(const EffectRequest& request) noexcept override;

    private:
        struct Impl;
        explicit EffectRenderer(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> impl_;
    };
}
