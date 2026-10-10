#include "ImGuiVectorUI/Backends/EffectsDX12.h"
#include "ImGuiVectorUI/Backends/ShaderLibrary.h"
#include "ItemEffectsInternal.h"

#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace IVUI::DX12
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        constexpr UINT kConstantCount = 32;

        bool ValidShader(const ShaderBinary& shader)
        {
            return shader.code != nullptr && shader.sizeBytes != 0;
        }

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
        struct ItemUpload
        {
            ComPtr<ID3D12Resource> vertices;
            ComPtr<ID3D12Resource> indices;
            std::size_t vertexCapacity = 0;
            std::size_t indexCapacity = 0;
        };

        struct FrameUploads
        {
            std::vector<ItemUpload> items;
            std::size_t nextItem = 0;
        };

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
        ComPtr<ID3D12PipelineState> itemCompositePipeline;
        ComPtr<ID3D12DescriptorHeap> srvHeap;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12Resource> source;
        ComPtr<ID3D12Resource> ping;
        ComPtr<ID3D12Resource> pong;
        std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 3> srv{};
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE sourceRtv{};
        std::vector<FrameUploads> uploadFrames;
        EffectExtent resourceExtent{};
        bool frameActive = false;
        bool captured = false;
        bool immutableBackdrop = false;
        float cachedBlurSigmaX = -1.0f;
        float cachedBlurSigmaY = -1.0f;
        D3D12_GPU_DESCRIPTOR_HANDLE cachedBlur{};
        std::string error;

        ShaderBinary FindShader(const char* filename) const noexcept
        {
            return ShaderLibrary::Find(ShaderBackend::DirectX, filename);
        }

        bool Initialize()
        {
            if (createInfo.device == nullptr || createInfo.targetFormat == DXGI_FORMAT_UNKNOWN ||
                createInfo.framesInFlight == 0)
            {
                error = "DX12 effects require a device, target format, and frames in flight.";
                return false;
            }
            uploadFrames.resize(createInfo.framesInFlight);

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

            const ShaderBinary vertex = FindShader("effects_fullscreen.vs.cso");
            if (!ValidShader(vertex))
            {
                error = "Missing DX12 effect shader: effects_fullscreen.vs.cso";
                return false;
            }

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = rootSignature.Get();
            pipelineDesc.VS = {vertex.code, vertex.sizeBytes};
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

            auto createPipeline = [&](const char* filename, bool alphaBlend, bool additive,
                                       ComPtr<ID3D12PipelineState>& output)
            {
                const ShaderBinary pixel = FindShader(filename);
                if (!ValidShader(pixel))
                {
                    error = std::string("Missing DX12 effect shader: ") + filename;
                    return false;
                }
                pipelineDesc.PS = {pixel.code, pixel.sizeBytes};
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
                    error = std::string("CreateGraphicsPipelineState failed for ") + filename + ".";
                    return false;
                }
                return true;
            };

            if (!createPipeline("effects_blur.ps.cso", false, false, blurPipeline) ||
                !createPipeline("effects_background.ps.cso", false, false, backgroundPipeline) ||
                !createPipeline("effects_material.ps.cso", true, false, glassPipeline) ||
                !createPipeline("effects_shape.ps.cso", true, false, shapePipeline) ||
                !createPipeline("effects_bloom.ps.cso", false, false, bloomFilterPipeline) ||
                !createPipeline("effects_texture.ps.cso", true, false, texturePipeline) ||
                !createPipeline("effects_composite.ps.cso", false, true, bloomCompositePipeline) ||
                !createPipeline("effects_composite.ps.cso", true, false, itemCompositePipeline))
                return false;

            D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
            srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvDesc.NumDescriptors = 3;
            srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NumDescriptors = 3;
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
            if (!createTexture(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, source) ||
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
            createInfo.device->CreateRenderTargetView(source.Get(), nullptr, rtvCpu);
            sourceRtv = rtvCpu;
            rtvCpu.ptr += rtvStride;
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

        bool EnsureUploadResource(ComPtr<ID3D12Resource>& resource,
                                  std::size_t& capacity, std::size_t required)
        {
            if (resource && capacity >= required)
                return true;
            resource.Reset();
            capacity = 0;

            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = (std::max)(required, static_cast<std::size_t>(64 * 1024));
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(createInfo.device->CreateCommittedResource(
                    &heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                    IID_PPV_ARGS(&resource))))
            {
                error = "Creating a DX12 item-effect upload buffer failed.";
                return false;
            }
            capacity = static_cast<std::size_t>(desc.Width);
            return true;
        }

        bool UploadItem(const ItemEffectDrawData& drawData,
                        D3D12_VERTEX_BUFFER_VIEW& vertexView,
                        D3D12_INDEX_BUFFER_VIEW& indexView)
        {
            const std::size_t vertexBytes = drawData.vertices.size() * sizeof(ImDrawVert);
            const std::size_t indexBytes = drawData.indices.size() * sizeof(std::uint32_t);
            if (vertexBytes > (std::numeric_limits<UINT>::max)() ||
                indexBytes > (std::numeric_limits<UINT>::max)())
            {
                error = "DX12 item-effect geometry exceeds buffer-view limits.";
                return false;
            }

            FrameUploads& frameUploads = uploadFrames[frame.frameIndex % uploadFrames.size()];
            const std::size_t itemIndex = frameUploads.nextItem++;
            if (itemIndex == frameUploads.items.size())
                frameUploads.items.emplace_back();
            ItemUpload& upload = frameUploads.items[itemIndex];
            if (!EnsureUploadResource(upload.vertices, upload.vertexCapacity, vertexBytes) ||
                !EnsureUploadResource(upload.indices, upload.indexCapacity, indexBytes))
                return false;

            D3D12_RANGE readRange{0, 0};
            void* mappedVertices = nullptr;
            if (FAILED(upload.vertices->Map(0, &readRange, &mappedVertices)))
            {
                error = "Mapping the DX12 item-effect vertex buffer failed.";
                return false;
            }
            std::memcpy(mappedVertices, drawData.vertices.data(), vertexBytes);
            D3D12_RANGE writtenRange{0, vertexBytes};
            upload.vertices->Unmap(0, &writtenRange);

            void* mappedIndices = nullptr;
            if (FAILED(upload.indices->Map(0, &readRange, &mappedIndices)))
            {
                error = "Mapping the DX12 item-effect index buffer failed.";
                return false;
            }
            std::memcpy(mappedIndices, drawData.indices.data(), indexBytes);
            writtenRange.End = indexBytes;
            upload.indices->Unmap(0, &writtenRange);

            vertexView.BufferLocation = upload.vertices->GetGPUVirtualAddress();
            vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
            vertexView.StrideInBytes = sizeof(ImDrawVert);
            indexView.BufferLocation = upload.indices->GetGPUVirtualAddress();
            indexView.SizeInBytes = static_cast<UINT>(indexBytes);
            indexView.Format = DXGI_FORMAT_R32_UINT;
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
                cachedBlurSigmaX = -1.0f;
                cachedBlurSigmaY = -1.0f;
                cachedBlur = {};
            }
            BindCommon();
            return true;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE Blur(float sigmaX, float sigmaY,
                                         D3D12_GPU_DESCRIPTOR_HANDLE input,
                                         std::size_t inputIndex)
        {
            if (sigmaX <= 0.01f && sigmaY <= 0.01f)
                return input;
            sigmaX = (std::max)(sigmaX, 0.01f);
            sigmaY = (std::max)(sigmaY, 0.01f);
            if (inputIndex == 0 && cachedBlur.ptr != 0 &&
                std::fabs(cachedBlurSigmaX - sigmaX) < 0.001f &&
                std::fabs(cachedBlurSigmaY - sigmaY) < 0.001f)
                return cachedBlur;
            const std::size_t firstTarget = inputIndex == 1 ? 1 : 0;
            const std::size_t secondTarget = firstTarget == 0 ? 1 : 0;
            ID3D12Resource* targets[] = {ping.Get(), pong.Get()};
            const EffectConstants horizontal = MakeBlurEffectConstants(
                frame.extent, {1.0f, 0.0f}, sigmaX);
            Transition(frame.commandList, targets[firstTarget], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
            Draw(blurPipeline.Get(), input, horizontal, rtv[firstTarget]);
            Transition(frame.commandList, targets[firstTarget], D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            const EffectConstants vertical = MakeBlurEffectConstants(
                frame.extent, {0.0f, 1.0f}, sigmaY);
            Transition(frame.commandList, targets[secondTarget], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
            Draw(blurPipeline.Get(), srv[firstTarget + 1], vertical, rtv[secondTarget]);
            Transition(frame.commandList, targets[secondTarget], D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            const D3D12_GPU_DESCRIPTOR_HANDLE result = srv[secondTarget + 1];
            if (inputIndex == 0)
            {
                cachedBlurSigmaX = sigmaX;
                cachedBlurSigmaY = sigmaY;
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
                    const auto blurred = Blur(request.blur.sigma, request.blur.sigma, srv[0], 0);
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
                    const auto blurred = Blur(
                        request.glass.blurSigma, request.glass.blurSigma, srv[0], 0);
                    Draw(glassPipeline.Get(), blurred,
                          MakeGlassEffectConstants(request.glass, frame.extent, frame.timeSeconds),
                          frame.targetRtv);
                    // A following panel needs a fresh Z-aware capture. The paired
                    // BackdropBlur -> Glass commands still share this panel's blur.
                    captured = false;
                    cachedBlurSigmaX = -1.0f;
                    cachedBlurSigmaY = -1.0f;
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
                    const auto blurred = Blur(
                        request.bloom.sigma, request.bloom.sigma, srv[1], 1);
                    Draw(bloomCompositePipeline.Get(), blurred, bloom, frame.targetRtv);
                    captured = false;
                    cachedBlurSigmaX = -1.0f;
                    cachedBlurSigmaY = -1.0f;
                    cachedBlur = {};
                }
                break;
            default:
                break;
            }
        }

        void RenderItem(const ItemEffectRequest& request,
                        const ItemEffectDrawData& drawData)
        {
            if (!frameActive || frame.commandList == nullptr || frame.drawData == nullptr ||
                drawData.Empty() ||
                request.layerCount == 0 || !EnsureResources())
                return;

            captured = false;
            cachedBlurSigmaX = -1.0f;
            cachedBlurSigmaY = -1.0f;
            cachedBlur = {};

            D3D12_VERTEX_BUFFER_VIEW vertexView{};
            D3D12_INDEX_BUFFER_VIEW indexView{};
            if (!UploadItem(drawData, vertexView, indexView))
                return;

            Transition(frame.commandList, source.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
            frame.commandList->OMSetRenderTargets(1, &sourceRtv, FALSE, nullptr);
            constexpr float clearColor[4] = {};
            frame.commandList->ClearRenderTargetView(sourceRtv, clearColor, 0, nullptr);
            SetFullscreenState(frame.commandList);
            frame.commandList->IASetVertexBuffers(0, 1, &vertexView);
            frame.commandList->IASetIndexBuffer(&indexView);
            if (frame.restoreSrvDescriptorHeap)
            {
                ID3D12DescriptorHeap* heaps[] = {frame.restoreSrvDescriptorHeap};
                frame.commandList->SetDescriptorHeaps(1, heaps);
            }

            const ImVec2 displayPos = frame.drawData->DisplayPos;
            const ImVec2 displaySize = frame.drawData->DisplaySize;
            const ImVec2 framebufferScale = frame.drawData->FramebufferScale;
            if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
            {
                Transition(frame.commandList, source.Get(),
                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                frame.commandList->OMSetRenderTargets(1, &frame.targetRtv, FALSE, nullptr);
                return;
            }
            const float left = displayPos.x;
            const float right = displayPos.x + displaySize.x;
            const float top = displayPos.y;
            const float bottom = displayPos.y + displaySize.y;
            const std::array<float, 16> projection = {
                2.0f / (right - left), 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                (right + left) / (left - right),
                (top + bottom) / (bottom - top), 0.5f, 1.0f};
            frame.commandList->SetGraphicsRoot32BitConstants(
                0, static_cast<UINT>(projection.size()), projection.data(), 0);
            for (const ItemEffectDrawCommand& command : drawData.commands)
            {
                if (command.indexCount == 0 || command.textureId == ImTextureID_Invalid ||
                    command.firstIndex > drawData.indices.size() ||
                    command.indexCount > drawData.indices.size() - command.firstIndex)
                    continue;
                const float minX = std::clamp(
                    (command.clipRect.x - displayPos.x) * framebufferScale.x,
                    0.0f, static_cast<float>(frame.extent.width));
                const float minY = std::clamp(
                    (command.clipRect.y - displayPos.y) * framebufferScale.y,
                    0.0f, static_cast<float>(frame.extent.height));
                const float maxX = std::clamp(
                    (command.clipRect.z - displayPos.x) * framebufferScale.x,
                    0.0f, static_cast<float>(frame.extent.width));
                const float maxY = std::clamp(
                    (command.clipRect.w - displayPos.y) * framebufferScale.y,
                    0.0f, static_cast<float>(frame.extent.height));
                if (maxX <= minX || maxY <= minY)
                    continue;

                const D3D12_RECT scissor{
                    static_cast<LONG>(minX), static_cast<LONG>(minY),
                    static_cast<LONG>(maxX), static_cast<LONG>(maxY)};
                frame.commandList->RSSetScissorRects(1, &scissor);
                D3D12_GPU_DESCRIPTOR_HANDLE texture{};
                texture.ptr = static_cast<UINT64>(command.textureId);
                frame.commandList->SetGraphicsRootDescriptorTable(1, texture);
                frame.commandList->DrawIndexedInstanced(
                    command.indexCount, 1, command.firstIndex, 0, 0);
            }
            Transition(frame.commandList, source.Get(),
                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            frame.commandList->OMSetRenderTargets(1, &frame.targetRtv, FALSE, nullptr);

            BindCommon();
            const float sigmaScaleX = std::fabs(framebufferScale.x);
            const float sigmaScaleY = std::fabs(framebufferScale.y);
            const std::uint32_t layerCount = std::min<std::uint32_t>(
                request.layerCount, static_cast<std::uint32_t>(request.layers.size()));
            for (std::uint32_t index = 0; index < layerCount; ++index)
            {
                const ItemEffectLayer& layer = request.layers[index];
                if (layer.type != ItemEffectType::Shadow &&
                    layer.type != ItemEffectType::Glow &&
                    layer.type != ItemEffectType::Blur)
                    continue;
                const D3D12_GPU_DESCRIPTOR_HANDLE blurred = Blur(
                    layer.sigma * sigmaScaleX, layer.sigma * sigmaScaleY, srv[0], 0);
                Draw(itemCompositePipeline.Get(), blurred,
                     MakeItemCompositeConstants(
                         layer, frame.extent, framebufferScale),
                     frame.targetRtv);
            }

            captured = false;
            cachedBlurSigmaX = -1.0f;
            cachedBlurSigmaY = -1.0f;
            cachedBlur = {};
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
            frameInfo.targetRtv.ptr == 0 || frameInfo.extent.width == 0 ||
            frameInfo.extent.height == 0 ||
            frameInfo.frameIndex >= impl_->uploadFrames.size())
            return false;
        impl_->frame = frameInfo;
        impl_->uploadFrames[frameInfo.frameIndex % impl_->uploadFrames.size()].nextItem = 0;
        impl_->captured = false;
        impl_->immutableBackdrop = frameInfo.renderBackground || frameInfo.backdrop != nullptr;
        impl_->cachedBlurSigmaX = -1.0f;
        impl_->cachedBlurSigmaY = -1.0f;
        impl_->cachedBlur = {};
        impl_->frameActive = true;
        if (frameInfo.renderBackground)
        {
            const bool rendered = impl_->RenderBackground() && impl_->Capture();
            impl_->RestoreHostDescriptorHeap();
            if (!rendered)
            {
                impl_->frameActive = false;
                return false;
            }
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

    void EffectRenderer::RenderItemEffects(const ItemEffectRequest& request,
                                           const ItemEffectDrawData& drawData) noexcept
    {
        if (!impl_)
            return;
        try
        {
            impl_->RenderItem(request, drawData);
            impl_->RestoreHostDescriptorHeap();
        }
        catch (...)
        {
            impl_->error = "Unexpected DX12 item-effect rendering failure.";
            impl_->RestoreHostDescriptorHeap();
        }
    }
}
