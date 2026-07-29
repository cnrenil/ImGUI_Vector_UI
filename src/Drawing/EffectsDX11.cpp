#include "ImGuiVectorUI/Backends/EffectsDX11.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace IVUI::DX11
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        const char* kShaderSource = R"hlsl(
Texture2D SourceTexture : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer Constants : register(b0) { float4 uData[16]; };
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };
VSOut FullscreenVS(uint id : SV_VertexID) {
    VSOut o; float2 p = float2(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0);
    o.position = float4(p, 0.0, 1.0); o.uv = float2((p.x + 1.0) * 0.5, 1.0 - (p.y + 1.0) * 0.5); return o;
}
float Sdf(float2 p, float2 mn, float2 mx, float r) {
    float2 c = (mn + mx) * 0.5, h = max((mx - mn) * 0.5, 0.0); r = min(max(r, 0.0), min(h.x, h.y));
    float2 q = abs(p - c) - h + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float4 BlurPS(VSOut i) : SV_Target {
    float4 sum = 0.0; float weights = 0.0; float sigma = max(uData[1].x, 0.5);
    [unroll] for (int x = -16; x <= 16; ++x) { float d = (float)x; float w = exp(-0.5 * d * d / (sigma * sigma));
        sum += SourceTexture.Sample(LinearClamp, i.uv + uData[0].zw * uData[0].xy * d) * w; weights += w; }
    return sum / max(weights, 0.0001);
}
float4 GlassPS(VSOut i) : SV_Target {
    float2 mn = uData[0].xy, mx = uData[0].zw; float radius = uData[1].x, opacity = saturate(uData[1].y);
    float3 tint = float3(uData[1].z, uData[1].w, uData[2].x); float tintAmount = saturate(uData[2].y);
    float3 accent = float3(uData[2].z, uData[2].w, uData[3].x); float sdf = Sdf(i.position.xy, mn, mx, radius);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf); float edge = mask * (1.0 - smoothstep(0.0, max(uData[3].z, 0.5), abs(sdf)));
    float2 n = float2(Sdf(i.position.xy + float2(1,0), mn,mx,radius)-Sdf(i.position.xy-float2(1,0),mn,mx,radius),
                      Sdf(i.position.xy + float2(0,1), mn,mx,radius)-Sdf(i.position.xy-float2(0,1),mn,mx,radius)); n = normalize(n + 0.0001);
    float2 sampleUv = clamp(i.uv + uData[7].xy * n * uData[3].y * edge, uData[7].xy * 0.5, 1.0-uData[7].xy*0.5);
    float3 material = lerp(SourceTexture.Sample(LinearClamp, sampleUv).rgb, tint, tintAmount);
    material += accent * edge * saturate(uData[3].w); float outside = max(sdf, 0.0);
    float glow = exp(-outside/max(uData[6].y,0.5))*saturate(uData[6].x)*(1.0-mask);
    float shadow = exp(-outside/max(uData[6].w,0.5))*saturate(uData[6].z)*(1.0-mask);
    float materialAlpha = mask * opacity, alpha = saturate(materialAlpha + glow + shadow); clip(alpha - 0.001);
    return float4(material * materialAlpha + accent * glow, alpha);
}
float4 ShapePS(VSOut i) : SV_Target {
    float sdf = Sdf(i.position.xy, uData[0].xy + uData[3].xy, uData[0].zw + uData[3].xy, uData[1].x);
    float sigma = max(uData[1].z, 0.5); float alpha = exp(-max(sdf,0.0)*max(sdf,0.0)/(2.0*sigma*sigma))*saturate(uData[1].y);
    clip(alpha - 0.001); return float4(uData[2].rgb * alpha, alpha);
}
float4 BloomFilterPS(VSOut i) : SV_Target {
    float4 c = SourceTexture.Sample(LinearClamp, i.uv); float hi=max(c.r,max(c.g,c.b)), lo=min(c.r,min(c.g,c.b));
    float m = smoothstep(uData[1].z-0.1,uData[1].z+0.05,hi)*smoothstep(uData[1].w-0.05,uData[1].w+0.05,hi-lo);
    m *= all(i.position.xy >= uData[2].xy) && all(i.position.xy <= uData[2].zw) ? 1.0 : 0.0; return c*m;
}
float4 TexturePS(VSOut i) : SV_Target {
    float sdf=Sdf(i.position.xy,uData[0].xy,uData[0].zw,uData[1].x);
    float mask=1.0-smoothstep(-0.5,1.0,sdf); clip(mask-0.001);
    float opacity=saturate(uData[1].y)*mask; float4 source=SourceTexture.Sample(LinearClamp,i.uv);
    return float4(source.rgb*opacity,opacity);
}
float4 BloomCompositePS(VSOut i) : SV_Target {
    float m=all(i.position.xy>=uData[2].xy)&&all(i.position.xy<=uData[2].zw)?1.0:0.0;
    return float4(SourceTexture.Sample(LinearClamp,i.uv).rgb*uData[1].y*m,0.0);
}
)hlsl";

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
        explicit Impl(const EffectRendererCreateInfo& info) : createInfo(info) {}

        EffectRendererCreateInfo createInfo{};
        EffectRendererFrameInfo frame{};
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> blurShader;
        ComPtr<ID3D11PixelShader> glassShader;
        ComPtr<ID3D11PixelShader> shapeShader;
        ComPtr<ID3D11PixelShader> bloomFilterShader;
        ComPtr<ID3D11PixelShader> textureShader;
        ComPtr<ID3D11PixelShader> bloomCompositeShader;
        ComPtr<ID3D11Buffer> constantBuffer;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11RasterizerState> rasterizer;
        ComPtr<ID3D11DepthStencilState> depthStencil;
        ComPtr<ID3D11BlendState> opaqueBlend;
        ComPtr<ID3D11BlendState> alphaBlend;
        ComPtr<ID3D11BlendState> additiveBlend;
        ComPtr<ID3D11Texture2D> source;
        ComPtr<ID3D11Texture2D> ping;
        ComPtr<ID3D11Texture2D> pong;
        std::array<ComPtr<ID3D11ShaderResourceView>, 3> srv;
        std::array<ComPtr<ID3D11RenderTargetView>, 2> rtv;
        EffectExtent resourceExtent{};
        DXGI_FORMAT resourceFormat = DXGI_FORMAT_UNKNOWN;
        bool frameActive = false;
        bool captured = false;
        float cachedBlurSigma = -1.0f;
        ID3D11ShaderResourceView* cachedBlur = nullptr;
        std::string error;

        ComPtr<ID3DBlob> Compile(const char* entry, const char* target)
        {
            ComPtr<ID3DBlob> code;
            ComPtr<ID3DBlob> errors;
            const HRESULT result = D3DCompile(kShaderSource, std::strlen(kShaderSource), "IVUI Effects",
                                              nullptr, nullptr, entry, target,
                                              D3DCOMPILE_ENABLE_STRICTNESS |
                                                  D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                              0, &code, &errors);
            if (FAILED(result))
            {
                error = errors ? static_cast<const char*>(errors->GetBufferPointer()) :
                                 "D3DCompile failed.";
                return {};
            }
            return code;
        }

        bool Initialize()
        {
            if (createInfo.device == nullptr)
            {
                error = "DX11 effects require a device.";
                return false;
            }
            const ComPtr<ID3DBlob> vertex = Compile("FullscreenVS", "vs_5_0");
            if (!vertex || FAILED(createInfo.device->CreateVertexShader(vertex->GetBufferPointer(),
                vertex->GetBufferSize(), nullptr, &vertexShader)))
            {
                if (error.empty()) error = "CreateVertexShader failed.";
                return false;
            }
            auto createPixelShader = [&](const char* entry, ComPtr<ID3D11PixelShader>& output)
            {
                const ComPtr<ID3DBlob> code = Compile(entry, "ps_5_0");
                if (!code)
                    return false;
                if (FAILED(createInfo.device->CreatePixelShader(code->GetBufferPointer(),
                                                                 code->GetBufferSize(), nullptr, &output)))
                {
                    error = std::string("CreatePixelShader failed for ") + entry + ".";
                    return false;
                }
                return true;
            };
            if (!createPixelShader("BlurPS", blurShader) ||
                !createPixelShader("GlassPS", glassShader) ||
                !createPixelShader("ShapePS", shapeShader) ||
                !createPixelShader("BloomFilterPS", bloomFilterShader) ||
                !createPixelShader("TexturePS", textureShader) ||
                !createPixelShader("BloomCompositePS", bloomCompositeShader))
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

            source.Reset(); ping.Reset(); pong.Reset();
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
            if (FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &ping)) ||
                FAILED(createInfo.device->CreateTexture2D(&desc, nullptr, &pong)))
                return FailResources();
            ID3D11Texture2D* textures[] = {source.Get(), ping.Get(), pong.Get()};
            for (std::size_t i = 0; i < std::size(textures); ++i)
                if (FAILED(createInfo.device->CreateShaderResourceView(textures[i], nullptr, &srv[i])))
                    return FailResources();
            if (FAILED(createInfo.device->CreateRenderTargetView(ping.Get(), nullptr, &rtv[0])) ||
                FAILED(createInfo.device->CreateRenderTargetView(pong.Get(), nullptr, &rtv[1])))
                return FailResources();
            resourceExtent = frame.extent;
            resourceFormat = targetDesc.Format;
            return true;
        }

        bool FailResources()
        {
            error = "Creating DX11 effect textures or views failed.";
            source.Reset(); ping.Reset(); pong.Reset();
            return false;
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
                captured = true;
                cachedBlurSigma = -1.0f;
                cachedBlur = nullptr;
            }
            SetCommonState();
            return true;
        }

        ID3D11ShaderResourceView* Blur(float sigma, ID3D11ShaderResourceView* input,
                                      std::size_t inputIndex)
        {
            if (sigma <= 0.01f)
                return input;
            if (inputIndex == 0 && cachedBlur && std::fabs(cachedBlurSigma - sigma) < 0.001f)
                return cachedBlur;
            const std::size_t firstTarget = inputIndex == 1 ? 1 : 0;
            const std::size_t secondTarget = firstTarget == 0 ? 1 : 0;
            Draw(blurShader.Get(), input,
                 MakeBlurEffectConstants(frame.extent, {1.0f, 0.0f}, sigma),
                 rtv[firstTarget].Get(), opaqueBlend.Get());
            Draw(blurShader.Get(), srv[firstTarget + 1].Get(),
                 MakeBlurEffectConstants(frame.extent, {0.0f, 1.0f}, sigma),
                 rtv[secondTarget].Get(), opaqueBlend.Get());
            ID3D11ShaderResourceView* result = srv[secondTarget + 1].Get();
            if (inputIndex == 0)
            {
                cachedBlurSigma = sigma;
                cachedBlur = result;
            }
            return result;
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
                    Draw(textureShader.Get(), Blur(request.blur.sigma, srv[0].Get(), 0), constants,
                          frame.targetRtv, alphaBlend.Get());
                }
                break;
            case EffectId::Glass:
                if (Capture())
                {
                    Draw(glassShader.Get(), Blur(request.glass.blurSigma, srv[0].Get(), 0),
                          MakeGlassEffectConstants(request.glass, frame.extent, frame.timeSeconds),
                          frame.targetRtv, alphaBlend.Get());
                    captured = false;
                    cachedBlurSigma = -1.0f;
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
                    Draw(bloomCompositeShader.Get(), Blur(request.bloom.sigma, srv[1].Get(), 1),
                         constants, frame.targetRtv, additiveBlend.Get());
                }
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

    bool EffectRenderer::BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept
    {
        if (!impl_ || frameInfo.deviceContext == nullptr || frameInfo.target == nullptr ||
            frameInfo.targetRtv == nullptr || frameInfo.extent.width == 0 || frameInfo.extent.height == 0)
            return false;
        impl_->frame = frameInfo;
        impl_->captured = false;
        impl_->cachedBlurSigma = -1.0f;
        impl_->cachedBlur = nullptr;
        impl_->frameActive = true;
        return true;
    }

    void EffectRenderer::EndFrame() noexcept
    {
        if (impl_)
        {
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
}
