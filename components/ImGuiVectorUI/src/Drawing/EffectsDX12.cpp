#include "ImGuiVectorUI/Backends/EffectsDX12.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <new>
#include <string>
#include <utility>

namespace IVUI::DX12
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        constexpr UINT kConstantCount = 32;

        const char* kShaderSource = R"hlsl(
Texture2D SourceTexture : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer EffectConstants : register(b0) { float4 uData[8]; };

struct VertexOutput { float4 position : SV_Position; float2 uv : TEXCOORD0; };

VertexOutput FullscreenVS(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 position = float2(vertexId == 2 ? 3.0 : -1.0, vertexId == 1 ? 3.0 : -1.0);
    output.position = float4(position, 0.0, 1.0);
    output.uv = float2((position.x + 1.0) * 0.5, 1.0 - (position.y + 1.0) * 0.5);
    return output;
}

float RoundedRectSdf(float2 pixelPosition, float2 rectMin, float2 rectMax, float radius)
{
    float2 center = (rectMin + rectMax) * 0.5;
    float2 halfSize = max((rectMax - rectMin) * 0.5, 0.0);
    radius = min(max(radius, 0.0), min(halfSize.x, halfSize.y));
    float2 q = abs(pixelPosition - center) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 BlurPS(VertexOutput input) : SV_Target
{
    float2 texel = uData[0].xy;
    float2 direction = uData[0].zw;
    float sigma = max(uData[1].x, 0.5);
    float4 sum = 0.0;
    float weightSum = 0.0;
    [unroll] for (int i = -16; i <= 16; ++i)
    {
        float distance = (float)i;
        float weight = exp(-0.5 * distance * distance / (sigma * sigma));
        sum += SourceTexture.Sample(LinearClamp, input.uv + direction * texel * distance) * weight;
        weightSum += weight;
    }
    return sum / max(weightSum, 0.0001);
}

float Hash21(float2 value)
{
    value = frac(value * float2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return frac(value.x * value.y);
}

float SmoothNoise21(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    float2 curve = local * local * (3.0 - 2.0 * local);
    float a = Hash21(cell);
    float b = Hash21(cell + float2(1.0, 0.0));
    float c = Hash21(cell + float2(0.0, 1.0));
    float d = Hash21(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, curve.x), lerp(c, d, curve.x), curve.y);
}

float LineMask(float2 coordinate)
{
    float2 width = max(fwidth(coordinate), 0.001);
    float2 distanceToLine = abs(frac(coordinate - 0.5) - 0.5) / width;
    return 1.0 - saturate(min(distanceToLine.x, distanceToLine.y));
}

float4 BackgroundPS(VertexOutput input) : SV_Target
{
    float3 topColor = uData[0].rgb;
    float3 bottomColor = uData[1].rgb;
    float3 accent = uData[2].rgb;
    float3 secondary = uData[3].rgb;
    float time = uData[4].x * uData[5].z;
    float gridSize = max(uData[4].y, 12.0);
    float gridStrength = saturate(uData[4].z);
    float vignetteStrength = saturate(uData[4].w);
    float2 resolution = max(uData[5].xy, 1.0);
    float intensity = max(uData[5].w, 0.0);

    float2 uv = input.uv;
    float2 centered = uv * 2.0 - 1.0;
    centered.x *= resolution.x / resolution.y;
    float3 color = lerp(topColor, bottomColor, smoothstep(0.0, 1.0, uv.y));

    float fineGrid = LineMask(input.position.xy / gridSize);
    float majorGrid = LineMask(input.position.xy / (gridSize * 5.0));
    color += secondary * (fineGrid * gridStrength * 0.28 + majorGrid * gridStrength * 0.68);

    float waveA = centered.y + 0.30 + sin(centered.x * 2.5 + time * 0.75) * 0.11;
    float waveB = centered.y - 0.20 + sin(centered.x * 4.1 - time * 0.58) * 0.075;
    color += accent * exp(-waveA * waveA * 105.0) * 0.58 * intensity;
    color += secondary * exp(-waveB * waveB * 155.0) * 0.48 * intensity;

    float2 beaconPosition = float2(sin(time * 0.42) * 0.72, cos(time * 0.31) * 0.34);
    float beacon = exp(-dot(centered - beaconPosition, centered - beaconPosition) * 11.0);
    float diagonal = exp(-pow(centered.y + centered.x * 0.34 - sin(time * 0.26) * 0.25, 2.0) * 75.0);
    color += accent * beacon * 0.52 * intensity;
    color += lerp(accent, secondary, uv.x) * diagonal * 0.20 * intensity;
    float vignette = 1.0 - smoothstep(0.30, 1.48, length(centered));
    color *= lerp(1.0, vignette, vignetteStrength);
    color += (Hash21(input.position.xy + floor(time * 12.0)) - 0.5) / 320.0;
    return float4(max(color, 0.0), 1.0);
}

float4 GlassPS(VertexOutput input) : SV_Target
{
    float2 rectMin = uData[0].xy;
    float2 rectMax = uData[0].zw;
    float radius = uData[1].x;
    float opacity = saturate(uData[1].y);
    float3 tint = uData[1].zwx;
    tint = float3(uData[1].z, uData[1].w, uData[2].x);
    float tintStrength = saturate(uData[2].y);
    float3 accent = float3(uData[2].z, uData[2].w, uData[3].x);
    float refraction = uData[3].y;
    float edgeWidth = max(uData[3].z, 0.5);
    float edgeHighlight = saturate(uData[3].w);
    float thickness = saturate(uData[4].x);
    float shapePower = max(uData[4].y, 2.0);
    float lensStrength = max(uData[4].z, 0.0);
    float lensBase = max(uData[4].w * 2.718281828, 1.0001);
    float lensFalloff = max(uData[5].x, 0.1);
    float lensBias = max(uData[5].y, 0.0);
    float lensPower = max(uData[5].z, 0.1);
    float noiseStrength = uData[5].w;
    float glowStrength = saturate(uData[6].x);
    float glowSpread = max(uData[6].y, 0.5);
    float shadowStrength = saturate(uData[6].z);
    float shadowSpread = max(uData[6].w, 0.5);
    float2 texel = uData[7].xy;
    float time = uData[7].z;

    float sdf = RoundedRectSdf(input.position.xy, rectMin, rectMax, radius);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);
    float edge = mask * (1.0 - smoothstep(0.0, edgeWidth, abs(sdf)));
    float epsilon = 1.0;
    float2 normal = float2(
        RoundedRectSdf(input.position.xy + float2(epsilon, 0.0), rectMin, rectMax, radius) -
            RoundedRectSdf(input.position.xy - float2(epsilon, 0.0), rectMin, rectMax, radius),
        RoundedRectSdf(input.position.xy + float2(0.0, epsilon), rectMin, rectMax, radius) -
            RoundedRectSdf(input.position.xy - float2(0.0, epsilon), rectMin, rectMax, radius));
    normal = normalize(normal + 0.0001);

    float2 center = (rectMin + rectMax) * 0.5;
    float2 halfSize = max((rectMax - rectMin) * 0.5, 1.0);
    float lensRadius = max(min(halfSize.x, halfSize.y), 1.0);
    float edgeDistance = saturate(max(-sdf, 0.0) / lensRadius);
    float interiorDistance = pow(edgeDistance, 2.0 / shapePower);
    float lensCompression = saturate(
        lensStrength * pow(lensBase, -lensFalloff * interiorDistance - lensBias));
    lensCompression = min(lensCompression, 0.98);
    float lensScale = pow(max(1.0 - lensCompression, 0.02), lensPower);
    float2 lensOffset = ((center + (input.position.xy - center) * lensScale) -
                         input.position.xy) * texel;

    float2 noisePosition = input.position.xy * 0.045 + float2(time * 0.35, -time * 0.23);
    float2 surfaceNoise = float2(SmoothNoise21(noisePosition),
                                 SmoothNoise21(noisePosition + 19.19)) * 2.0 - 1.0;
    float rimRefraction = refraction * edge * (0.35 + thickness * 0.65);
    float2 sampleUv = input.uv + lensOffset + texel *
        (normal * rimRefraction + surfaceNoise * noiseStrength * (0.25 + lensCompression * 0.75));
    sampleUv = clamp(sampleUv, texel * 0.5, 1.0 - texel * 0.5);
    float3 backdrop = SourceTexture.Sample(LinearClamp, sampleUv).rgb;
    float3 material = lerp(backdrop, tint, tintStrength * (0.7 + thickness * 0.3));
    float lightFacing = saturate(dot(normal, normalize(float2(-0.45, -0.90))));
    float3 rimColor = lerp(accent, 1.0, 0.55);
    material += rimColor * edge * (0.22 + lightFacing * 0.78) * edgeHighlight;

    float outside = max(sdf, 0.0);
    float shadow = exp(-outside / shadowSpread) * shadowStrength * (1.0 - mask);
    float glow = exp(-outside / glowSpread) * glowStrength * (1.0 - mask);
    float materialAlpha = mask * opacity;
    float alpha = saturate(materialAlpha + shadow + glow);
    clip(alpha - 0.001);
    float3 color = material * materialAlpha + accent * glow;
    return float4(color, alpha);
}

float4 ShapePS(VertexOutput input) : SV_Target
{
    float2 rectMin = uData[0].xy;
    float2 rectMax = uData[0].zw;
    float radius = uData[1].x;
    float strength = saturate(uData[1].y);
    float sigma = max(uData[1].z, 0.5);
    float3 color = uData[2].rgb;
    float2 offset = uData[3].xy;
    float sdf = RoundedRectSdf(input.position.xy, rectMin + offset, rectMax + offset, radius);
    float outside = max(sdf, 0.0);
    float outsideMask = smoothstep(-0.5, 0.5, sdf);
    float alpha = exp(-outside * outside / (2.0 * sigma * sigma)) *
                  outsideMask * strength * uData[2].a;
    clip(alpha - 0.001);
    return float4(color * alpha, alpha);
}

float4 BloomFilterPS(VertexOutput input) : SV_Target
{
    float4 source = SourceTexture.Sample(LinearClamp, input.uv);
    float brightness = max(source.r, max(source.g, source.b));
    float minimum = min(source.r, min(source.g, source.b));
    float brightnessWeight = smoothstep(uData[1].z - 0.1, uData[1].z + 0.05, brightness);
    float saturationWeight = smoothstep(uData[1].w - 0.05, uData[1].w + 0.05, brightness - minimum);
    float2 pixelPosition = input.position.xy;
    float boundsMask = all(pixelPosition >= uData[2].xy) && all(pixelPosition <= uData[2].zw) ? 1.0 : 0.0;
    return source * brightnessWeight * saturationWeight * boundsMask;
}

float4 TexturePS(VertexOutput input) : SV_Target
{
    float2 pixelPosition = input.position.xy;
    float sdf = RoundedRectSdf(pixelPosition, uData[0].xy, uData[0].zw, uData[1].x);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);
    clip(mask - 0.001);
    float opacity = saturate(uData[1].y) * mask;
    float4 source = SourceTexture.Sample(LinearClamp, input.uv);
    return float4(source.rgb * opacity, opacity);
}

float4 BloomCompositePS(VertexOutput input) : SV_Target
{
    float2 pixelPosition = input.position.xy;
    float boundsMask = all(pixelPosition >= uData[2].xy) && all(pixelPosition <= uData[2].zw) ? 1.0 : 0.0;
    return float4(SourceTexture.Sample(LinearClamp, input.uv).rgb * uData[1].y * boundsMask, 0.0);
}
)hlsl";

        void Transition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
                        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            if (before == after)
                return;
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            commandList->ResourceBarrier(1, &barrier);
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
    }

    bool ValidateEffectShaders(std::string* error) noexcept
    {
        constexpr const char* shaders[][2] = {
            {"FullscreenVS", "vs_5_0"}, {"BlurPS", "ps_5_0"},
            {"BackgroundPS", "ps_5_0"}, {"GlassPS", "ps_5_0"},
            {"ShapePS", "ps_5_0"}, {"BloomFilterPS", "ps_5_0"},
            {"TexturePS", "ps_5_0"}, {"BloomCompositePS", "ps_5_0"}};
        for (const auto& shader : shaders)
        {
            ComPtr<ID3DBlob> code;
            ComPtr<ID3DBlob> errors;
            const HRESULT result = D3DCompile(
                kShaderSource, std::char_traits<char>::length(kShaderSource), "IVUI Effects",
                nullptr, nullptr, shader[0], shader[1], D3DCOMPILE_ENABLE_STRICTNESS, 0,
                &code, &errors);
            if (FAILED(result))
            {
                if (error)
                {
                    *error = std::string(shader[0]) + ": " +
                        (errors ? static_cast<const char*>(errors->GetBufferPointer()) :
                                  "D3DCompile failed.");
                }
                return false;
            }
        }
        if (error) error->clear();
        return true;
    }

    struct EffectRenderer::Impl
    {
        explicit Impl(const EffectRendererCreateInfo& info) : createInfo(info) {}

        EffectRendererCreateInfo createInfo{};
        EffectRendererFrameInfo frame{};
        ComPtr<ID3D12RootSignature> rootSignature;
        ComPtr<ID3D12PipelineState> blurPipeline;
        ComPtr<ID3D12PipelineState> backgroundPipeline;
        ComPtr<ID3D12PipelineState> glassPipeline;
        ComPtr<ID3D12PipelineState> shapePipeline;
        ComPtr<ID3D12PipelineState> bloomFilterPipeline;
        ComPtr<ID3D12PipelineState> texturePipeline;
        ComPtr<ID3D12PipelineState> bloomCompositePipeline;
        ComPtr<ID3D12DescriptorHeap> srvHeap;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12Resource> source;
        ComPtr<ID3D12Resource> ping;
        ComPtr<ID3D12Resource> pong;
        std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 3> srv{};
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtv{};
        EffectExtent resourceExtent{};
        bool frameActive = false;
        bool captured = false;
        bool immutableBackdrop = false;
        float cachedBlurSigma = -1.0f;
        D3D12_GPU_DESCRIPTOR_HANDLE cachedBlur{};
        std::string error;

        ComPtr<ID3DBlob> Compile(const char* entry, const char* target)
        {
            ComPtr<ID3DBlob> code;
            ComPtr<ID3DBlob> errors;
            const HRESULT result = D3DCompile(kShaderSource, std::char_traits<char>::length(kShaderSource),
                                              "IVUI Effects", nullptr, nullptr, entry, target,
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
            if (createInfo.device == nullptr || createInfo.targetFormat == DXGI_FORMAT_UNKNOWN)
            {
                error = "DX12 effects require a device and target format.";
                return false;
            }

            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = 1;
            range.BaseShaderRegister = 0;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            D3D12_ROOT_PARAMETER parameters[2]{};
            parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            parameters[0].DescriptorTable.NumDescriptorRanges = 1;
            parameters[0].DescriptorTable.pDescriptorRanges = &range;
            parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            parameters[1].Constants.ShaderRegister = 0;
            parameters[1].Constants.Num32BitValues = kConstantCount;

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.MaxAnisotropy = 1;
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            sampler.MinLOD = 0.0f;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            D3D12_ROOT_SIGNATURE_DESC rootDesc{};
            rootDesc.NumParameters = 2;
            rootDesc.pParameters = parameters;
            rootDesc.NumStaticSamplers = 1;
            rootDesc.pStaticSamplers = &sampler;

            ComPtr<ID3DBlob> rootBlob;
            ComPtr<ID3DBlob> rootErrors;
            HRESULT result = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                         &rootBlob, &rootErrors);
            if (FAILED(result))
            {
                error = rootErrors ? static_cast<const char*>(rootErrors->GetBufferPointer()) :
                                     "D3D12SerializeRootSignature failed.";
                return false;
            }
            result = createInfo.device->CreateRootSignature(createInfo.nodeMask,
                rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
            if (FAILED(result))
            {
                char message[96]{};
                std::snprintf(message, sizeof(message), "CreateRootSignature failed (HRESULT 0x%08lX).",
                              static_cast<unsigned long>(result));
                error = message;
                return false;
            }

            ComPtr<ID3DBlob> vertex = Compile("FullscreenVS", "vs_5_0");
            if (!vertex)
                return false;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = rootSignature.Get();
            pipelineDesc.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
            pipelineDesc.SampleMask = UINT_MAX;
            pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
            pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pipelineDesc.NumRenderTargets = 1;
            pipelineDesc.RTVFormats[0] = createInfo.targetFormat;
            pipelineDesc.SampleDesc.Count = 1;
            pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask =
                D3D12_COLOR_WRITE_ENABLE_ALL;

            auto createPipeline = [&](const char* entry, bool alphaBlend, bool additive,
                                      ComPtr<ID3D12PipelineState>& output)
            {
                ComPtr<ID3DBlob> pixel = Compile(entry, "ps_5_0");
                if (!pixel)
                    return false;
                pipelineDesc.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
                auto& blend = pipelineDesc.BlendState.RenderTarget[0];
                blend.BlendEnable = alphaBlend || additive;
                blend.SrcBlend = D3D12_BLEND_ONE;
                blend.DestBlend = additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOp = D3D12_BLEND_OP_ADD;
                blend.SrcBlendAlpha = D3D12_BLEND_ONE;
                blend.DestBlendAlpha = additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                if (FAILED(createInfo.device->CreateGraphicsPipelineState(&pipelineDesc,
                                                                           IID_PPV_ARGS(&output))))
                {
                    error = std::string("CreateGraphicsPipelineState failed for ") + entry + ".";
                    return false;
                }
                return true;
            };

            if (!createPipeline("BlurPS", false, false, blurPipeline) ||
                !createPipeline("BackgroundPS", false, false, backgroundPipeline) ||
                !createPipeline("GlassPS", true, false, glassPipeline) ||
                !createPipeline("ShapePS", true, false, shapePipeline) ||
                !createPipeline("BloomFilterPS", false, false, bloomFilterPipeline) ||
                !createPipeline("TexturePS", true, false, texturePipeline) ||
                !createPipeline("BloomCompositePS", false, true, bloomCompositePipeline))
                return false;

            D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
            srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvDesc.NumDescriptors = 3;
            srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NumDescriptors = 2;
            if (FAILED(createInfo.device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvHeap))) ||
                FAILED(createInfo.device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap))))
            {
                error = "Creating DX12 effect descriptor heaps failed.";
                return false;
            }
            return true;
        }

        bool EnsureResources()
        {
            if (source && resourceExtent.width == frame.extent.width &&
                resourceExtent.height == frame.extent.height)
                return true;
            source.Reset();
            ping.Reset();
            pong.Reset();
            if (frame.extent.width == 0 || frame.extent.height == 0)
            {
                error = "DX12 effect extent must be non-zero.";
                return false;
            }

            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = frame.extent.width;
            desc.Height = frame.extent.height;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = createInfo.targetFormat;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            auto createTexture = [&](D3D12_RESOURCE_FLAGS flags, ComPtr<ID3D12Resource>& output)
            {
                desc.Flags = flags;
                return SUCCEEDED(createInfo.device->CreateCommittedResource(
                    &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    nullptr, IID_PPV_ARGS(&output)));
            };
            if (!createTexture(D3D12_RESOURCE_FLAG_NONE, source) ||
                !createTexture(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, ping) ||
                !createTexture(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, pong))
            {
                error = "Creating DX12 effect textures failed.";
                source.Reset();
                ping.Reset();
                pong.Reset();
                return false;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = createInfo.targetFormat;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            const UINT srvStride = createInfo.device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
            ID3D12Resource* textures[] = {source.Get(), ping.Get(), pong.Get()};
            for (std::size_t i = 0; i < std::size(textures); ++i)
            {
                createInfo.device->CreateShaderResourceView(textures[i], &srvDesc, srvCpu);
                srv[i] = srvGpu;
                srvCpu.ptr += srvStride;
                srvGpu.ptr += srvStride;
            }

            const UINT rtvStride = createInfo.device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            ID3D12Resource* renderTargets[] = {ping.Get(), pong.Get()};
            for (std::size_t i = 0; i < std::size(renderTargets); ++i)
            {
                createInfo.device->CreateRenderTargetView(renderTargets[i], nullptr, rtvCpu);
                rtv[i] = rtvCpu;
                rtvCpu.ptr += rtvStride;
            }
            resourceExtent = frame.extent;
            return true;
        }

        void SetFullscreenState(ID3D12GraphicsCommandList* commandList)
        {
            const D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(frame.extent.width),
                                          static_cast<float>(frame.extent.height), 0.0f, 1.0f};
            const D3D12_RECT scissor{0, 0, static_cast<LONG>(frame.extent.width),
                                     static_cast<LONG>(frame.extent.height)};
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }

        void BindCommon()
        {
            ID3D12DescriptorHeap* heaps[] = {srvHeap.Get()};
            frame.commandList->SetDescriptorHeaps(1, heaps);
            frame.commandList->SetGraphicsRootSignature(rootSignature.Get());
            SetFullscreenState(frame.commandList);
        }

        void RestoreHostDescriptorHeap()
        {
            if (frame.restoreSrvDescriptorHeap)
            {
                ID3D12DescriptorHeap* heaps[] = {frame.restoreSrvDescriptorHeap};
                frame.commandList->SetDescriptorHeaps(1, heaps);
            }
        }

        EffectConstants MakeBackgroundConstants() const
        {
            EffectConstants constants{};
            const BackgroundShaderParams& background = frame.background;
            const EffectColor colors[] = {
                background.topColor, background.bottomColor,
                background.accent, background.secondary};
            for (std::size_t index = 0; index < std::size(colors); ++index)
            {
                constants.values[index * 4 + 0] = colors[index].r;
                constants.values[index * 4 + 1] = colors[index].g;
                constants.values[index * 4 + 2] = colors[index].b;
                constants.values[index * 4 + 3] = colors[index].a;
            }
            constants.values[16] = frame.timeSeconds;
            constants.values[17] = background.gridSize;
            constants.values[18] = background.gridStrength;
            constants.values[19] = background.vignette;
            constants.values[20] = static_cast<float>(frame.extent.width);
            constants.values[21] = static_cast<float>(frame.extent.height);
            constants.values[22] = background.animationSpeed;
            constants.values[23] = background.intensity;
            return constants;
        }

        bool RenderBackground()
        {
            if (!EnsureResources())
                return false;
            BindCommon();
            Draw(backgroundPipeline.Get(), srv[0], MakeBackgroundConstants(), frame.targetRtv);
            return true;
        }

        void Draw(ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv,
                  const EffectConstants& constants, D3D12_CPU_DESCRIPTOR_HANDLE outputRtv)
        {
            frame.commandList->OMSetRenderTargets(1, &outputRtv, FALSE, nullptr);
            frame.commandList->SetPipelineState(pipeline);
            frame.commandList->SetGraphicsRootDescriptorTable(0, sourceSrv);
            frame.commandList->SetGraphicsRoot32BitConstants(1, kConstantCount,
                                                              constants.values.data(), 0);
            frame.commandList->DrawInstanced(3, 1, 0, 0);
        }

        bool Capture(bool forceTarget = false)
        {
            if (!EnsureResources())
                return false;
            if (!captured || forceTarget || !immutableBackdrop)
            {
                ID3D12Resource* capture = forceTarget || !frame.backdrop ? frame.target : frame.backdrop;
                const D3D12_RESOURCE_STATES captureState = forceTarget || !frame.backdrop
                    ? frame.targetState : frame.backdropState;
                Transition(frame.commandList, capture, captureState, D3D12_RESOURCE_STATE_COPY_SOURCE);
                Transition(frame.commandList, source.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_COPY_DEST);
                frame.commandList->CopyResource(source.Get(), capture);
                Transition(frame.commandList, source.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                Transition(frame.commandList, capture, D3D12_RESOURCE_STATE_COPY_SOURCE, captureState);
                captured = true;
                cachedBlurSigma = -1.0f;
                cachedBlur = {};
            }
            BindCommon();
            return true;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE Blur(float sigma, D3D12_GPU_DESCRIPTOR_HANDLE input,
                                        std::size_t inputIndex)
        {
            if (sigma <= 0.01f)
                return input;
            if (inputIndex == 0 && cachedBlur.ptr != 0 && std::fabs(cachedBlurSigma - sigma) < 0.001f)
                return cachedBlur;
            const std::size_t firstTarget = inputIndex == 1 ? 1 : 0;
            const std::size_t secondTarget = firstTarget == 0 ? 1 : 0;
            ID3D12Resource* targets[] = {ping.Get(), pong.Get()};
            const EffectConstants horizontal = MakeBlurEffectConstants(frame.extent, {1.0f, 0.0f}, sigma);
            Transition(frame.commandList, targets[firstTarget], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
            Draw(blurPipeline.Get(), input, horizontal, rtv[firstTarget]);
            Transition(frame.commandList, targets[firstTarget], D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            const EffectConstants vertical = MakeBlurEffectConstants(frame.extent, {0.0f, 1.0f}, sigma);
            Transition(frame.commandList, targets[secondTarget], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
            Draw(blurPipeline.Get(), srv[firstTarget + 1], vertical, rtv[secondTarget]);
            Transition(frame.commandList, targets[secondTarget], D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            const D3D12_GPU_DESCRIPTOR_HANDLE result = srv[secondTarget + 1];
            if (inputIndex == 0)
            {
                cachedBlurSigma = sigma;
                cachedBlur = result;
            }
            return result;
        }

        void Render(const EffectRequest& request)
        {
            if (!frameActive || frame.commandList == nullptr || frame.target == nullptr)
                return;

            switch (request.id)
            {
            case EffectId::BackdropBlur:
                // Capture at this draw-list depth so overlapping glass sees UI
                // already rendered beneath it instead of the frame-start backdrop.
                if (Capture(true))
                {
                    const auto blurred = Blur(request.blur.sigma, srv[0], 0);
                    EffectConstants constants{};
                    constants.values[0] = request.blur.bounds.min.x;
                    constants.values[1] = request.blur.bounds.min.y;
                    constants.values[2] = request.blur.bounds.max.x;
                    constants.values[3] = request.blur.bounds.max.y;
                    constants.values[4] = request.blur.rounding;
                    constants.values[5] = request.blur.opacity;
                    Draw(texturePipeline.Get(), blurred, constants, frame.targetRtv);
                }
                break;
            case EffectId::Glass:
                if (Capture())
                {
                    const auto blurred = Blur(request.glass.blurSigma, srv[0], 0);
                    Draw(glassPipeline.Get(), blurred,
                          MakeGlassEffectConstants(request.glass, frame.extent, frame.timeSeconds),
                          frame.targetRtv);
                    // A following panel needs a fresh Z-aware capture. The paired
                    // BackdropBlur -> Glass commands still share this panel's blur.
                    captured = false;
                    cachedBlurSigma = -1.0f;
                    cachedBlur = {};
                }
                break;
            case EffectId::Shadow:
                if (EnsureResources())
                {
                    BindCommon();
                    Draw(shapePipeline.Get(), srv[0],
                         MakeShapeConstants(request.shadow.bounds, request.shadow.rounding,
                                            request.shadow.strength, request.shadow.sigma,
                                            request.shadow.color, request.shadow.offset),
                         frame.targetRtv);
                }
                break;
            case EffectId::Glow:
                if (EnsureResources())
                {
                    BindCommon();
                    Draw(shapePipeline.Get(), srv[0],
                         MakeShapeConstants(request.glow.bounds, request.glow.rounding,
                                            request.glow.strength, request.glow.sigma,
                                            request.glow.color, {}),
                         frame.targetRtv);
                }
                break;
            case EffectId::Bloom:
                if (Capture(true))
                {
                    const EffectConstants bloom = MakeBloomEffectConstants(request.bloom, frame.extent);
                    Transition(frame.commandList, ping.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                               D3D12_RESOURCE_STATE_RENDER_TARGET);
                    Draw(bloomFilterPipeline.Get(), srv[0], bloom, rtv[0]);
                    Transition(frame.commandList, ping.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    const auto blurred = Blur(request.bloom.sigma, srv[1], 1);
                    Draw(bloomCompositePipeline.Get(), blurred, bloom, frame.targetRtv);
                    captured = false;
                    cachedBlurSigma = -1.0f;
                    cachedBlur = {};
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
            std::string shaderError;
            if (!ValidateEffectShaders(&shaderError))
            {
                if (error) *error = shaderError;
                return nullptr;
            }
            auto impl = std::make_unique<Impl>(createInfo);
            if (!impl->Initialize())
            {
                if (error) *error = impl->error;
                return nullptr;
            }
            if (error) error->clear();
            return std::unique_ptr<EffectRenderer>(new EffectRenderer(std::move(impl)));
        }
        catch (const std::exception& exception)
        {
            if (error) *error = exception.what();
            return nullptr;
        }
        catch (...)
        {
            if (error) *error = "Unexpected DX12 effect renderer initialization failure.";
            return nullptr;
        }
    }

    EffectRenderer::EffectRenderer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
    EffectRenderer::~EffectRenderer() = default;
    EffectRenderer::EffectRenderer(EffectRenderer&&) noexcept = default;
    EffectRenderer& EffectRenderer::operator=(EffectRenderer&&) noexcept = default;

    bool EffectRenderer::BeginFrame(const EffectRendererFrameInfo& frameInfo) noexcept
    {
        if (!impl_ || frameInfo.commandList == nullptr || frameInfo.target == nullptr ||
            frameInfo.targetRtv.ptr == 0 || frameInfo.extent.width == 0 || frameInfo.extent.height == 0)
            return false;
        impl_->frame = frameInfo;
        impl_->captured = false;
        impl_->immutableBackdrop = frameInfo.renderBackground || frameInfo.backdrop != nullptr;
        impl_->cachedBlurSigma = -1.0f;
        impl_->cachedBlur = {};
        impl_->frameActive = true;
        if (frameInfo.renderBackground)
        {
            if (!impl_->RenderBackground() || !impl_->Capture())
            {
                impl_->frameActive = false;
                return false;
            }
            impl_->RestoreHostDescriptorHeap();
        }
        return true;
    }

    void EffectRenderer::EndFrame() noexcept
    {
        if (impl_)
        {
            if (impl_->frameActive && impl_->frame.renderBloom)
                impl_->Render(EffectRequest::Bloom(impl_->frame.bloom));
            impl_->RestoreHostDescriptorHeap();
            impl_->frame = {};
            impl_->frameActive = false;
        }
    }

    const char* EffectRenderer::GetLastError() const noexcept
    {
        return impl_ ? impl_->error.c_str() : "DX12 effect renderer is not initialized.";
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
            impl_->RestoreHostDescriptorHeap();
        }
        catch (...)
        {
            impl_->error = "Unexpected DX12 effect rendering failure.";
            impl_->RestoreHostDescriptorHeap();
        }
    }
}
