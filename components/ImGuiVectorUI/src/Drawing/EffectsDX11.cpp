#include "ImGuiVectorUI/Backends/EffectsDX11.h"
#include "ImGuiVectorUI/Backends/ShaderLibrary.h"
#include "ItemEffectsInternal.h"

#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace IVUI::DX11
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        bool ValidShader(const ShaderBinary& shader)
        {
            return shader.code != nullptr && shader.sizeBytes != 0;
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
            constants.values[11] = color.a;
            constants.values[12] = offset.x;
            constants.values[13] = offset.y;
            return constants;
        }

        EffectConstants MakeItemCompositeConstants(const ItemEffectLayer& layer,
                                                    EffectExtent extent,
                                                    ImVec2 framebufferScale)
        {
            EffectConstants constants{};
            constants.values[0] = layer.color.r;
            constants.values[1] = layer.color.g;
            constants.values[2] = layer.color.b;
            constants.values[3] = layer.color.a;
            constants.values[4] = extent.width != 0
                ? layer.offset.x * framebufferScale.x / static_cast<float>(extent.width) : 0.0f;
            constants.values[5] = extent.height != 0
                ? layer.offset.y * framebufferScale.y / static_cast<float>(extent.height) : 0.0f;
            constants.values[6] = layer.strength;
            constants.values[7] = layer.opacity;
            constants.values[8] = static_cast<float>(layer.type);
            constants.values[31] = 1.0f;
            return constants;
        }

    }

    struct EffectRenderer::Impl
    {
        explicit Impl(const EffectRendererCreateInfo& info) : createInfo(info) {}

        EffectRendererCreateInfo createInfo{};
        EffectRendererFrameInfo frame{};
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> backgroundShader;
        ComPtr<ID3D11PixelShader> blurShader;
        ComPtr<ID3D11PixelShader> glassShader;
        ComPtr<ID3D11PixelShader> shapeShader;
        ComPtr<ID3D11PixelShader> bloomFilterShader;
        ComPtr<ID3D11PixelShader> textureShader;
        ComPtr<ID3D11PixelShader> bloomCompositeShader;
        ComPtr<ID3D11Buffer> constantBuffer;
        ComPtr<ID3D11Buffer> itemVertexBuffer;
        ComPtr<ID3D11Buffer> itemIndexBuffer;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11RasterizerState> rasterizer;
        ComPtr<ID3D11RasterizerState> itemRasterizer;
        ComPtr<ID3D11DepthStencilState> depthStencil;
        ComPtr<ID3D11BlendState> opaqueBlend;
        ComPtr<ID3D11BlendState> alphaBlend;
        ComPtr<ID3D11BlendState> additiveBlend;
        ComPtr<ID3D11BlendState> itemCaptureBlend;
        ComPtr<ID3D11Texture2D> source;
        ComPtr<ID3D11Texture2D> itemSource;
        ComPtr<ID3D11Texture2D> ping;
        ComPtr<ID3D11Texture2D> pong;
        ComPtr<ID3D11ShaderResourceView> itemSourceSrv;
        ComPtr<ID3D11RenderTargetView> itemSourceRtv;
        std::array<ComPtr<ID3D11ShaderResourceView>, 3> srv;
        std::array<ComPtr<ID3D11RenderTargetView>, 2> rtv;
        EffectExtent resourceExtent{};
        DXGI_FORMAT resourceFormat = DXGI_FORMAT_UNKNOWN;
        std::size_t itemVertexCapacity = 0;
        std::size_t itemIndexCapacity = 0;
        bool frameActive = false;
        bool captured = false;
        float cachedBlurSigmaX = -1.0f;
        float cachedBlurSigmaY = -1.0f;
        ID3D11ShaderResourceView* cachedBlur = nullptr;
        std::string error;

        ShaderBinary FindShader(const char* filename) const noexcept
        {
            return ShaderLibrary::Find(ShaderBackend::DirectX, filename);
        }

        bool InitializeItemPipeline()
        {
            D3D11_RASTERIZER_DESC rasterizerDesc{};
            rasterizerDesc.FillMode = D3D11_FILL_SOLID;
            rasterizerDesc.CullMode = D3D11_CULL_NONE;
            rasterizerDesc.ScissorEnable = TRUE;
            rasterizerDesc.DepthClipEnable = TRUE;
            if (FAILED(createInfo.device->CreateRasterizerState(
                    &rasterizerDesc, &itemRasterizer)))
            {
                error = "Creating the DX11 item effect rasterizer state failed.";
                return false;
            }

            D3D11_BLEND_DESC blendDesc{};
            auto& target = blendDesc.RenderTarget[0];
            target.BlendEnable = TRUE;
            target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            target.BlendOp = D3D11_BLEND_OP_ADD;
            target.SrcBlendAlpha = D3D11_BLEND_ONE;
            target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (FAILED(createInfo.device->CreateBlendState(
                    &blendDesc, &itemCaptureBlend)))
            {
                error = "Creating the DX11 item effect blend state failed.";
                return false;
            }
            return true;
        }

        bool Initialize()
        {
            if (createInfo.device == nullptr)
            {
                error = "DX11 effects require a device.";
                return false;
            }
            const ShaderBinary vertex = FindShader("effects_fullscreen.vs.cso");
            if (!ValidShader(vertex) || FAILED(createInfo.device->CreateVertexShader(
                vertex.code, vertex.sizeBytes, nullptr, &vertexShader)))
            {
                error = "Creating the DX11 fullscreen vertex shader failed.";
                return false;
            }
            auto createPixelShader = [&](const char* filename, ComPtr<ID3D11PixelShader>& output)
            {
                const ShaderBinary code = FindShader(filename);
                if (!ValidShader(code))
                {
                    error = std::string("Missing DX11 effect shader: ") + filename;
                    return false;
                }
                if (FAILED(createInfo.device->CreatePixelShader(
                    code.code, code.sizeBytes, nullptr, &output)))
                {
                    error = std::string("Creating the DX11 effect shader failed: ") + filename;
                    return false;
                }
                return true;
            };
            if (!createPixelShader("effects_background.ps.cso", backgroundShader) ||
                !createPixelShader("effects_blur.ps.cso", blurShader) ||
                !createPixelShader("effects_material.ps.cso", glassShader) ||
                !createPixelShader("effects_shape.ps.cso", shapeShader) ||
                !createPixelShader("effects_bloom.ps.cso", bloomFilterShader) ||
                !createPixelShader("effects_texture.ps.cso", textureShader) ||
                !createPixelShader("effects_composite.ps.cso", bloomCompositeShader))
                return false;

            if (!InitializeItemPipeline())
                return false;

            D3D11_BUFFER_DESC bufferDesc{};
            bufferDesc.ByteWidth = sizeof(EffectConstants);
            bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(createInfo.device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer)))
            {
                error = "Creating the DX11 effect constant buffer failed.";
                return false;
            }
            D3D11_SAMPLER_DESC samplerDesc{};
            samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
            if (FAILED(createInfo.device->CreateSamplerState(&samplerDesc, &sampler)))
            {
                error = "Creating the DX11 effect sampler failed.";
                return false;
            }
            D3D11_RASTERIZER_DESC rasterizerDesc{};
            rasterizerDesc.FillMode = D3D11_FILL_SOLID;
            rasterizerDesc.CullMode = D3D11_CULL_NONE;
            rasterizerDesc.DepthClipEnable = TRUE;
            if (FAILED(createInfo.device->CreateRasterizerState(&rasterizerDesc, &rasterizer)))
                return false;
            D3D11_DEPTH_STENCIL_DESC depthDesc{};
            depthDesc.DepthEnable = FALSE;
            depthDesc.StencilEnable = FALSE;
            if (FAILED(createInfo.device->CreateDepthStencilState(&depthDesc, &depthStencil)))
                return false;

            auto createBlend = [&](bool enabled, bool additive, ComPtr<ID3D11BlendState>& output)
            {
                D3D11_BLEND_DESC desc{};
                auto& target = desc.RenderTarget[0];
                target.BlendEnable = enabled;
                target.SrcBlend = D3D11_BLEND_ONE;
                target.DestBlend = additive ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
                target.BlendOp = D3D11_BLEND_OP_ADD;
                target.SrcBlendAlpha = D3D11_BLEND_ONE;
                target.DestBlendAlpha = additive ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
                target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
                target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                return SUCCEEDED(createInfo.device->CreateBlendState(&desc, &output));
            };
            if (!createBlend(false, false, opaqueBlend) || !createBlend(true, false, alphaBlend) ||
                !createBlend(true, true, additiveBlend))
            {
                error = "Creating DX11 effect blend states failed.";
                return false;
            }
            return true;
        }

        bool EnsureResources()
        {
            D3D11_TEXTURE2D_DESC targetDesc{};
            frame.target->GetDesc(&targetDesc);
            if (targetDesc.SampleDesc.Count != 1)
            {
                error = "DX11 effects require a non-MSAA target.";
                return false;
            }
            if (source && resourceExtent.width == frame.extent.width &&
                resourceExtent.height == frame.extent.height && resourceFormat == targetDesc.Format)
                return true;

            source.Reset(); itemSource.Reset(); ping.Reset(); pong.Reset();
            itemSourceSrv.Reset(); itemSourceRtv.Reset();
            for (auto& view : srv) view.Reset();
            for (auto& view : rtv) view.Reset();
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = frame.extent.width;
            desc.Height = frame.extent.height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = targetDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &source)))
                return FailResources();
            desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            if (FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &itemSource)) ||
                FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &ping)) ||
                FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &pong)))
                return FailResources();
            ID3D11Texture2D* textures[] = {source.Get(), ping.Get(), pong.Get()};
            for (std::size_t i = 0; i < std::size(textures); ++i)
                if (FAILED(createInfo.device->CreateShaderResourceView(textures[i], nullptr, &srv[i])))
                    return FailResources();
            if (FAILED(createInfo.device->CreateShaderResourceView(
                    itemSource.Get(), nullptr, &itemSourceSrv)) ||
                FAILED(createInfo.device->CreateRenderTargetView(
                    itemSource.Get(), nullptr, &itemSourceRtv)) ||
                FAILED(createInfo.device->CreateRenderTargetView(ping.Get(), nullptr, &rtv[0])) ||
                FAILED(createInfo.device->CreateRenderTargetView(pong.Get(), nullptr, &rtv[1])))
                return FailResources();
            resourceExtent = frame.extent;
            resourceFormat = targetDesc.Format;
            return true;
        }

        bool FailResources()
        {
            error = "Creating DX11 effect textures or views failed.";
            source.Reset(); itemSource.Reset(); ping.Reset(); pong.Reset();
            itemSourceSrv.Reset(); itemSourceRtv.Reset();
            for (auto& view : srv) view.Reset();
            for (auto& view : rtv) view.Reset();
            return false;
        }

        bool RenderBackground()
        {
            if (!EnsureResources())
                return false;
            SetCommonState();
            Draw(backgroundShader.Get(), nullptr,
                 MakeBackgroundEffectConstants(
                     frame.background, frame.extent, frame.timeSeconds),
                 frame.targetRtv, opaqueBlend.Get());
            return true;
        }

        void SetCommonState()
        {
            const D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(frame.extent.width),
                                          static_cast<float>(frame.extent.height), 0.0f, 1.0f};
            frame.deviceContext->RSSetViewports(1, &viewport);
            frame.deviceContext->RSSetState(rasterizer.Get());
            frame.deviceContext->OMSetDepthStencilState(depthStencil.Get(), 0);
            frame.deviceContext->IASetInputLayout(nullptr);
            frame.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            frame.deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
            frame.deviceContext->GSSetShader(nullptr, nullptr, 0);
            ID3D11SamplerState* samplerValue = sampler.Get();
            frame.deviceContext->PSSetSamplers(0, 1, &samplerValue);
            ID3D11Buffer* buffer = constantBuffer.Get();
            frame.deviceContext->PSSetConstantBuffers(0, 1, &buffer);
        }

        void Draw(ID3D11PixelShader* shader, ID3D11ShaderResourceView* input,
                  const EffectConstants& constants, ID3D11RenderTargetView* output,
                  ID3D11BlendState* blend)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(frame.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                                                0, &mapped)))
                return;
            std::memcpy(mapped.pData, constants.values.data(), sizeof(constants));
            frame.deviceContext->Unmap(constantBuffer.Get(), 0);
            frame.deviceContext->OMSetRenderTargets(1, &output, nullptr);
            frame.deviceContext->OMSetBlendState(blend, nullptr, UINT_MAX);
            frame.deviceContext->PSSetShader(shader, nullptr, 0);
            frame.deviceContext->PSSetShaderResources(0, 1, &input);
            frame.deviceContext->Draw(3, 0);
            ID3D11ShaderResourceView* nullView = nullptr;
            frame.deviceContext->PSSetShaderResources(0, 1, &nullView);
        }

        bool Capture(bool forceTarget = false)
        {
            if (!EnsureResources())
                return false;
            if (!captured || forceTarget)
            {
                frame.deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
                frame.deviceContext->CopyResource(source.Get(),
                    forceTarget || !frame.backdrop ? frame.target : frame.backdrop);
                frame.deviceContext->OMSetRenderTargets(1, &frame.targetRtv, nullptr);
                captured = true;
                cachedBlurSigmaX = -1.0f;
                cachedBlurSigmaY = -1.0f;
                cachedBlur = nullptr;
            }
            SetCommonState();
            return true;
        }

        ID3D11ShaderResourceView* Blur(float sigmaX, float sigmaY,
                                       ID3D11ShaderResourceView* input,
                                       std::size_t inputIndex)
        {
            if (sigmaX <= 0.01f && sigmaY <= 0.01f)
                return input;
            sigmaX = (std::max)(sigmaX, 0.01f);
            sigmaY = (std::max)(sigmaY, 0.01f);
            if (inputIndex == 0 && cachedBlur &&
                std::fabs(cachedBlurSigmaX - sigmaX) < 0.001f &&
                std::fabs(cachedBlurSigmaY - sigmaY) < 0.001f)
                return cachedBlur;
            const std::size_t firstTarget = inputIndex == 1 ? 1 : 0;
            const std::size_t secondTarget = firstTarget == 0 ? 1 : 0;
            Draw(blurShader.Get(), input,
                  MakeBlurEffectConstants(frame.extent, {1.0f, 0.0f}, sigmaX),
                  rtv[firstTarget].Get(), opaqueBlend.Get());
            Draw(blurShader.Get(), srv[firstTarget + 1].Get(),
                  MakeBlurEffectConstants(frame.extent, {0.0f, 1.0f}, sigmaY),
                  rtv[secondTarget].Get(), opaqueBlend.Get());
            ID3D11ShaderResourceView* result = srv[secondTarget + 1].Get();
            if (inputIndex == 0)
            {
                cachedBlurSigmaX = sigmaX;
                cachedBlurSigmaY = sigmaY;
                cachedBlur = result;
            }
            return result;
        }

        void ResetBlurCache()
        {
            cachedBlurSigmaX = -1.0f;
            cachedBlurSigmaY = -1.0f;
            cachedBlur = nullptr;
        }

        bool EnsureItemBuffers(std::size_t vertexCount, std::size_t indexCount)
        {
            if (vertexCount > UINT_MAX / sizeof(ImDrawVert) ||
                indexCount > UINT_MAX / sizeof(std::uint32_t))
            {
                error = "DX11 item effect geometry exceeds buffer limits.";
                return false;
            }
            if (!itemVertexBuffer || itemVertexCapacity < vertexCount)
            {
                itemVertexBuffer.Reset();
                itemVertexCapacity = vertexCount;
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth = static_cast<UINT>(itemVertexCapacity * sizeof(ImDrawVert));
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                if (FAILED(createInfo.device->CreateBuffer(&desc, nullptr, &itemVertexBuffer)))
                {
                    itemVertexCapacity = 0;
                    error = "Creating the DX11 item effect vertex buffer failed.";
                    return false;
                }
            }
            if (!itemIndexBuffer || itemIndexCapacity < indexCount)
            {
                itemIndexBuffer.Reset();
                itemIndexCapacity = indexCount;
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth = static_cast<UINT>(itemIndexCapacity * sizeof(std::uint32_t));
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                if (FAILED(createInfo.device->CreateBuffer(&desc, nullptr, &itemIndexBuffer)))
                {
                    itemIndexCapacity = 0;
                    error = "Creating the DX11 item effect index buffer failed.";
                    return false;
                }
            }
            return true;
        }

        bool UploadItemGeometry(const ItemEffectDrawData& drawData)
        {
            if (!EnsureItemBuffers(drawData.vertices.size(), drawData.indices.size()))
                return false;
            D3D11_MAPPED_SUBRESOURCE vertices{};
            if (FAILED(frame.deviceContext->Map(itemVertexBuffer.Get(), 0,
                                                D3D11_MAP_WRITE_DISCARD, 0, &vertices)))
            {
                error = "Uploading DX11 item effect vertices failed.";
                return false;
            }
            D3D11_MAPPED_SUBRESOURCE indices{};
            if (FAILED(frame.deviceContext->Map(itemIndexBuffer.Get(), 0,
                                                D3D11_MAP_WRITE_DISCARD, 0, &indices)))
            {
                frame.deviceContext->Unmap(itemVertexBuffer.Get(), 0);
                error = "Uploading DX11 item effect indices failed.";
                return false;
            }
            std::memcpy(vertices.pData, drawData.vertices.data(),
                        drawData.vertices.size() * sizeof(ImDrawVert));
            std::memcpy(indices.pData, drawData.indices.data(),
                        drawData.indices.size() * sizeof(std::uint32_t));
            frame.deviceContext->Unmap(itemVertexBuffer.Get(), 0);
            frame.deviceContext->Unmap(itemIndexBuffer.Get(), 0);
            return true;
        }

        bool RenderItemSource(const ItemEffectDrawData& item)
        {
            if (!EnsureResources() || !UploadItemGeometry(item) || !frame.drawData)
                return false;

            ID3D11ShaderResourceView* nullView = nullptr;
            frame.deviceContext->PSSetShaderResources(0, 1, &nullView);
            ID3D11RenderTargetView* output = itemSourceRtv.Get();
            frame.deviceContext->OMSetRenderTargets(1, &output, nullptr);
            constexpr float clearColor[4] = {};
            frame.deviceContext->ClearRenderTargetView(itemSourceRtv.Get(), clearColor);

            const ImDrawData& drawData = *frame.drawData;
            const D3D11_VIEWPORT viewport{
                0.0f, 0.0f,
                drawData.DisplaySize.x * drawData.FramebufferScale.x,
                drawData.DisplaySize.y * drawData.FramebufferScale.y,
                0.0f, 1.0f};
            frame.deviceContext->RSSetViewports(1, &viewport);
            frame.deviceContext->RSSetState(itemRasterizer.Get());
            frame.deviceContext->OMSetDepthStencilState(depthStencil.Get(), 0);
            frame.deviceContext->OMSetBlendState(itemCaptureBlend.Get(), nullptr, UINT_MAX);

            const UINT stride = sizeof(ImDrawVert);
            const UINT offset = 0;
            ID3D11Buffer* vertexBuffer = itemVertexBuffer.Get();
            frame.deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            frame.deviceContext->IASetIndexBuffer(itemIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            frame.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            const ImVec2 clipOffset = drawData.DisplayPos;
            const ImVec2 clipScale = drawData.FramebufferScale;
            for (const ItemEffectDrawCommand& command : item.commands)
            {
                if (command.indexCount == 0 || command.textureId == ImTextureID_Invalid ||
                    command.firstIndex > item.indices.size() ||
                    command.indexCount > item.indices.size() - command.firstIndex)
                    continue;
                const ImVec2 clipMin(
                    std::clamp((command.clipRect.x - clipOffset.x) * clipScale.x,
                               0.0f, static_cast<float>(frame.extent.width)),
                    std::clamp((command.clipRect.y - clipOffset.y) * clipScale.y,
                               0.0f, static_cast<float>(frame.extent.height)));
                const ImVec2 clipMax(
                    std::clamp((command.clipRect.z - clipOffset.x) * clipScale.x,
                               0.0f, static_cast<float>(frame.extent.width)),
                    std::clamp((command.clipRect.w - clipOffset.y) * clipScale.y,
                               0.0f, static_cast<float>(frame.extent.height)));
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;
                const D3D11_RECT scissor{
                    static_cast<LONG>(clipMin.x), static_cast<LONG>(clipMin.y),
                    static_cast<LONG>(clipMax.x), static_cast<LONG>(clipMax.y)};
                frame.deviceContext->RSSetScissorRects(1, &scissor);
                auto* texture = reinterpret_cast<ID3D11ShaderResourceView*>(
                    static_cast<std::uintptr_t>(command.textureId));
                frame.deviceContext->PSSetShaderResources(0, 1, &texture);
                frame.deviceContext->DrawIndexed(command.indexCount, command.firstIndex, 0);
            }
            frame.deviceContext->PSSetShaderResources(0, 1, &nullView);
            frame.deviceContext->OMSetRenderTargets(1, &frame.targetRtv, nullptr);
            return true;
        }

        void RenderItem(const ItemEffectRequest& request, const ItemEffectDrawData& item)
        {
            if (!frameActive || !frame.drawData || item.Empty() || request.layerCount == 0)
                return;

            ResetBlurCache();
            if (!RenderItemSource(item))
            {
                frame.deviceContext->OMSetRenderTargets(1, &frame.targetRtv, nullptr);
                ResetBlurCache();
                return;
            }

            SetCommonState();
            const float scaleX = std::fabs(frame.drawData->FramebufferScale.x);
            const float scaleY = std::fabs(frame.drawData->FramebufferScale.y);
            const std::uint32_t layerCount = std::min<std::uint32_t>(
                request.layerCount, static_cast<std::uint32_t>(request.layers.size()));
            for (std::uint32_t index = 0; index < layerCount; ++index)
            {
                const ItemEffectLayer& layer = request.layers[index];
                if (layer.type != ItemEffectType::Shadow &&
                    layer.type != ItemEffectType::Glow &&
                    layer.type != ItemEffectType::Blur)
                    continue;
                ID3D11ShaderResourceView* blurred = Blur(
                    layer.sigma * scaleX, layer.sigma * scaleY,
                    itemSourceSrv.Get(), 0);
                Draw(bloomCompositeShader.Get(), blurred,
                     MakeItemCompositeConstants(
                         layer, frame.extent, frame.drawData->FramebufferScale),
                      frame.targetRtv, alphaBlend.Get());
            }
            captured = false;
            ResetBlurCache();
        }

        void Render(const EffectRequest& request)
        {
            if (!frameActive)
                return;
            switch (request.id)
            {
            case EffectId::BackdropBlur:
                // Preserve immediate-mode Z ordering for overlapping windows.
                if (Capture(true))
                {
                    EffectConstants constants{};
                    constants.values[0] = request.blur.bounds.min.x;
                    constants.values[1] = request.blur.bounds.min.y;
                    constants.values[2] = request.blur.bounds.max.x;
                    constants.values[3] = request.blur.bounds.max.y;
                    constants.values[4] = request.blur.rounding;
                    constants.values[5] = request.blur.opacity;
                    Draw(textureShader.Get(),
                          Blur(request.blur.sigma, request.blur.sigma, srv[0].Get(), 0), constants,
                          frame.targetRtv, alphaBlend.Get());
                }
                break;
            case EffectId::Glass:
                if (Capture())
                {
                    Draw(glassShader.Get(),
                          Blur(request.glass.blurSigma, request.glass.blurSigma, srv[0].Get(), 0),
                          MakeGlassEffectConstants(request.glass, frame.extent, frame.timeSeconds),
                          frame.targetRtv, alphaBlend.Get());
                    captured = false;
                    cachedBlurSigmaX = -1.0f;
                    cachedBlurSigmaY = -1.0f;
                    cachedBlur = nullptr;
                }
                break;
            case EffectId::Shadow:
                if (EnsureResources())
                {
                    SetCommonState();
                    Draw(shapeShader.Get(), srv[0].Get(),
                         MakeShapeConstants(request.shadow.bounds, request.shadow.rounding,
                                            request.shadow.strength, request.shadow.sigma,
                                            request.shadow.color, request.shadow.offset),
                         frame.targetRtv, alphaBlend.Get());
                }
                break;
            case EffectId::Glow:
                if (EnsureResources())
                {
                    SetCommonState();
                    Draw(shapeShader.Get(), srv[0].Get(),
                         MakeShapeConstants(request.glow.bounds, request.glow.rounding,
                                            request.glow.strength, request.glow.sigma,
                                            request.glow.color, {}),
                         frame.targetRtv, alphaBlend.Get());
                }
                break;
            case EffectId::Bloom:
                if (Capture(true))
                {
                    const EffectConstants constants = MakeBloomEffectConstants(request.bloom, frame.extent);
                    Draw(bloomFilterShader.Get(), srv[0].Get(), constants, rtv[0].Get(), opaqueBlend.Get());
                    Draw(bloomCompositeShader.Get(),
                          Blur(request.bloom.sigma, request.bloom.sigma, srv[1].Get(), 1),
                          constants, frame.targetRtv, additiveBlend.Get());
                }
                break;
            default:
                break;
            }
        }
    };

    std::unique_ptr<EffectRenderer> EffectRenderer::Create(
        const EffectRendererCreateInfo& createInfo, std::string* error) noexcept
    {
        try
        {
            auto impl = std::make_unique<Impl>(createInfo);
            if (!impl->Initialize())
            {
                if (error) *error = impl->error;
                return nullptr;
            }
            if (error) error->clear();
            return std::unique_ptr<EffectRenderer>(new EffectRenderer(std::move(impl)));
        }
        catch (...)
        {
            if (error) *error = "Unexpected DX11 effect renderer initialization failure.";
            return nullptr;
        }
    }

    EffectRenderer::EffectRenderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
    EffectRenderer::~EffectRenderer() = default;
    EffectRenderer::EffectRenderer(EffectRenderer&&) noexcept = default;
    EffectRenderer& EffectRenderer::operator=(EffectRenderer&&) noexcept = default;

    bool EffectRenderer::BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept
    {
        if (!impl_ || frameInfo.deviceContext == nullptr || frameInfo.target == nullptr ||
            frameInfo.targetRtv == nullptr || frameInfo.extent.width == 0 || frameInfo.extent.height == 0)
            return false;
        impl_->frame = frameInfo;
        impl_->captured = false;
        impl_->cachedBlurSigmaX = -1.0f;
        impl_->cachedBlurSigmaY = -1.0f;
        impl_->cachedBlur = nullptr;
        impl_->frameActive = true;
        if (frameInfo.renderBackground &&
            (!impl_->RenderBackground() || !impl_->Capture(true)))
        {
            impl_->frameActive = false;
            return false;
        }
        return true;
    }

    void EffectRenderer::EndFrame() noexcept
    {
        if (impl_)
        {
            impl_->ResetBlurCache();
            impl_->frame = {};
            impl_->frameActive = false;
        }
    }

    const char* EffectRenderer::GetLastError() const noexcept
    {
        return impl_ ? impl_->error.c_str() : "DX11 effect renderer is not initialized.";
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
            impl_->error = "Unexpected DX11 effect rendering failure.";
        }
    }

    void EffectRenderer::RenderItemEffects(const ItemEffectRequest& request,
                                           const ItemEffectDrawData& drawData) noexcept
    {
        if (!impl_)
            return;
        try
        {
            impl_->RenderItem(request, drawData);
        }
        catch (...)
        {
            if (impl_->frameActive && impl_->frame.deviceContext && impl_->frame.targetRtv)
                impl_->frame.deviceContext->OMSetRenderTargets(
                    1, &impl_->frame.targetRtv, nullptr);
            impl_->ResetBlurCache();
            impl_->error = "Unexpected DX11 item effect rendering failure.";
        }
    }
}
