/** @file EffectsDX11.h
 *  @brief Direct3D 11 implementation of the IVUI effect renderer.
 */
#pragma once

#include "ImGuiVectorUI/Drawing/Effects.h"

#include <d3d11.h>

#include <memory>
#include <string>

namespace IVUI::DX11
{
    using BackgroundShaderParams = IVUI::BackgroundShaderParams;

    struct EffectRendererCreateInfo
    {
        ID3D11Device* device = nullptr;
    };

    struct EffectRendererFrameInfo
    {
        ID3D11DeviceContext* deviceContext = nullptr;
        ID3D11Texture2D* target = nullptr;
        ID3D11RenderTargetView* targetRtv = nullptr;
        EffectExtent extent{};
        float timeSeconds = 0.0f;
        ID3D11Texture2D* backdrop = nullptr;
        bool renderBackground = false;
        BackgroundShaderParams background{};
        const ImDrawData* drawData = nullptr;
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
        void RenderItemEffects(const ItemEffectRequest& request,
                               const ItemEffectDrawData& drawData) noexcept override;

    private:
        struct Impl;
        explicit EffectRenderer(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> impl_;
    };
}
