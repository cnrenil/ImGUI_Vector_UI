// Dear ImGui + Win32 + DirectX 12 host for a menu-like overlay demo.

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include "ImGuiVectorUI/ImGuiVectorUI.h"
#include "DemoMenu.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <tchar.h>
#include <algorithm>
#include <cstring>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

    struct GlassEffectsState
    {
        ID3D12Device* device = nullptr;
        ID3D12RootSignature* rootSignature = nullptr;
        ID3D12PipelineState* gaussianPipeline = nullptr;
        ID3D12PipelineState* backdropGaussianPipeline = nullptr;
        ID3D12PipelineState* backgroundPipeline = nullptr;
        ID3D12PipelineState* filterPipeline = nullptr;
        ID3D12PipelineState* compositePipeline = nullptr;
        ID3D12PipelineState* uiCompositePipeline = nullptr;
        ID3D12PipelineState* glowPipeline = nullptr;
        ID3D12DescriptorHeap* srvHeap = nullptr;
        ID3D12DescriptorHeap* rtvHeap = nullptr;
        ID3D12Resource* sourceTexture = nullptr;
        ID3D12Resource* backgroundTexture = nullptr;
        ID3D12Resource* uiTexture = nullptr;
        ID3D12Resource* filterTexture = nullptr;
        ID3D12Resource* horizontalBlurTexture = nullptr;
        ID3D12Resource* verticalBlurTexture = nullptr;
        DXGI_FORMAT outputFormat = DXGI_FORMAT_UNKNOWN;
        UINT outputWidth = 0;
        UINT outputHeight = 0;
        UINT srvDescriptorStride = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE uiSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE filterSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE horizontalBlurSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE verticalBlurSrv{};
        D3D12_CPU_DESCRIPTOR_HANDLE backgroundRtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE uiRtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE filterRtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE horizontalBlurRtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE verticalBlurRtv{};
    };
    static GlassEffectsState g_glassEffects;

    static void ReleaseTexture(ID3D12Resource*& value) { if (value) { value->Release(); value = nullptr; } }
    static void ReleaseGlassTextures() { ReleaseTexture(g_glassEffects.sourceTexture); ReleaseTexture(g_glassEffects.backgroundTexture); ReleaseTexture(g_glassEffects.uiTexture); ReleaseTexture(g_glassEffects.filterTexture); ReleaseTexture(g_glassEffects.horizontalBlurTexture); ReleaseTexture(g_glassEffects.verticalBlurTexture); g_glassEffects.outputWidth = g_glassEffects.outputHeight = 0; }
    static void Transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        if (before == after) return;
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = { resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after };
        cmd->ResourceBarrier(1, &barrier);
    }
    static void SetFullscreenState(ID3D12GraphicsCommandList* cmd, UINT width, UINT height)
    {
        D3D12_VIEWPORT vp{ 0, 0, (float)width, (float)height, 0, 1 }; D3D12_RECT sc{ 0, 0, (LONG)width, (LONG)height };
        cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &sc); cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
    static bool CreateGlassTexture(UINT width, UINT height, D3D12_RESOURCE_FLAGS flags, ID3D12Resource** out)
    {
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width = width; desc.Height = height;
        desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.Format = g_glassEffects.outputFormat; desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; desc.Flags = flags;
        return SUCCEEDED(g_glassEffects.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(out)));
    }
    static bool EnsureGlassTextures(UINT width, UINT height)
    {
        if (g_glassEffects.sourceTexture && g_glassEffects.outputWidth == width && g_glassEffects.outputHeight == height) return true;
        ReleaseGlassTextures();
        if (!CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_NONE, &g_glassEffects.sourceTexture) ||
            !CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &g_glassEffects.backgroundTexture) ||
            !CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &g_glassEffects.uiTexture) ||
            !CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &g_glassEffects.filterTexture) ||
            !CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &g_glassEffects.horizontalBlurTexture) ||
            !CreateGlassTexture(width, height, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &g_glassEffects.verticalBlurTexture)) { ReleaseGlassTextures(); return false; }
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format = g_glassEffects.outputFormat; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = g_glassEffects.srvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = g_glassEffects.srvHeap->GetGPUDescriptorHandleForHeapStart();
        auto createSrv = [&](ID3D12Resource* texture, D3D12_GPU_DESCRIPTOR_HANDLE& handle)
        {
            g_glassEffects.device->CreateShaderResourceView(texture, &srv, cpu);
            handle = gpu;
            cpu.ptr += g_glassEffects.srvDescriptorStride;
            gpu.ptr += g_glassEffects.srvDescriptorStride;
        };
        createSrv(g_glassEffects.sourceTexture, g_glassEffects.sourceSrv);
        createSrv(g_glassEffects.uiTexture, g_glassEffects.uiSrv);
        createSrv(g_glassEffects.filterTexture, g_glassEffects.filterSrv);
        createSrv(g_glassEffects.horizontalBlurTexture, g_glassEffects.horizontalBlurSrv);
        createSrv(g_glassEffects.verticalBlurTexture, g_glassEffects.verticalBlurSrv);
        g_glassEffects.backgroundRtv = g_glassEffects.rtvHeap->GetCPUDescriptorHandleForHeapStart(); g_glassEffects.uiRtv = g_glassEffects.backgroundRtv;
        const UINT rtvStride = g_glassEffects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        g_glassEffects.uiRtv.ptr += rtvStride; g_glassEffects.filterRtv = g_glassEffects.uiRtv; g_glassEffects.filterRtv.ptr += rtvStride;
        g_glassEffects.horizontalBlurRtv = g_glassEffects.filterRtv; g_glassEffects.horizontalBlurRtv.ptr += rtvStride; g_glassEffects.verticalBlurRtv = g_glassEffects.horizontalBlurRtv; g_glassEffects.verticalBlurRtv.ptr += rtvStride;
        g_glassEffects.device->CreateRenderTargetView(g_glassEffects.backgroundTexture, nullptr, g_glassEffects.backgroundRtv); g_glassEffects.device->CreateRenderTargetView(g_glassEffects.uiTexture, nullptr, g_glassEffects.uiRtv); g_glassEffects.device->CreateRenderTargetView(g_glassEffects.filterTexture, nullptr, g_glassEffects.filterRtv); g_glassEffects.device->CreateRenderTargetView(g_glassEffects.horizontalBlurTexture, nullptr, g_glassEffects.horizontalBlurRtv); g_glassEffects.device->CreateRenderTargetView(g_glassEffects.verticalBlurTexture, nullptr, g_glassEffects.verticalBlurRtv);
        g_glassEffects.outputWidth = width; g_glassEffects.outputHeight = height; return true;
    }
    static ID3DBlob* CompileGlassShader(const char* entry, const char* target)
    {
        const char* source = IVUI::EffectsShaderSource(); ID3DBlob* code = nullptr; ID3DBlob* errors = nullptr;
        const HRESULT hr = D3DCompile(source, std::strlen(source), "IVUI Effects", nullptr, nullptr, entry, target,
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
        if (errors)
        {
            ::OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            errors->Release();
        }
        if (FAILED(hr)) { if (code) code->Release(); return nullptr; } return code;
    }
    static void ShutdownGlassEffects()
    {
        ReleaseGlassTextures(); if (g_glassEffects.rtvHeap) { g_glassEffects.rtvHeap->Release(); g_glassEffects.rtvHeap = nullptr; } if (g_glassEffects.srvHeap) { g_glassEffects.srvHeap->Release(); g_glassEffects.srvHeap = nullptr; }
        if (g_glassEffects.glowPipeline) { g_glassEffects.glowPipeline->Release(); g_glassEffects.glowPipeline = nullptr; }
        if (g_glassEffects.uiCompositePipeline) { g_glassEffects.uiCompositePipeline->Release(); g_glassEffects.uiCompositePipeline = nullptr; }
        if (g_glassEffects.compositePipeline) { g_glassEffects.compositePipeline->Release(); g_glassEffects.compositePipeline = nullptr; }
        if (g_glassEffects.filterPipeline) { g_glassEffects.filterPipeline->Release(); g_glassEffects.filterPipeline = nullptr; }
        if (g_glassEffects.backgroundPipeline) { g_glassEffects.backgroundPipeline->Release(); g_glassEffects.backgroundPipeline = nullptr; }
        if (g_glassEffects.backdropGaussianPipeline) { g_glassEffects.backdropGaussianPipeline->Release(); g_glassEffects.backdropGaussianPipeline = nullptr; }
        if (g_glassEffects.gaussianPipeline) { g_glassEffects.gaussianPipeline->Release(); g_glassEffects.gaussianPipeline = nullptr; }
        if (g_glassEffects.rootSignature) { g_glassEffects.rootSignature->Release(); g_glassEffects.rootSignature = nullptr; }
        g_glassEffects.device = nullptr; g_glassEffects.outputFormat = DXGI_FORMAT_UNKNOWN; g_glassEffects.srvDescriptorStride = 0;
    }
    static bool InitGlassEffects(ID3D12Device* device, DXGI_FORMAT format)
    {
        if (!device || format == DXGI_FORMAT_UNKNOWN) return false;
        g_glassEffects.device = device; g_glassEffects.outputFormat = format;
        D3D12_DESCRIPTOR_RANGE range{}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
        D3D12_ROOT_PARAMETER params[2]{}; params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[0].DescriptorTable.NumDescriptorRanges = 1; params[0].DescriptorTable.pDescriptorRanges = &range;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; params[1].Constants.ShaderRegister = 0; params[1].Constants.Num32BitValues = 44;
        D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; sampler.ShaderRegister = 0; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; sampler.MaxLOD = D3D12_FLOAT32_MAX;
        D3D12_ROOT_SIGNATURE_DESC root{}; root.NumParameters = 2; root.pParameters = params; root.NumStaticSamplers = 1; root.pStaticSamplers = &sampler;
        ID3DBlob* blob = nullptr;
        ID3DBlob* errors = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)))
        {
            if (errors)
            {
                ::OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                errors->Release();
            }
            return false;
        }
        const HRESULT rootHr = g_glassEffects.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&g_glassEffects.rootSignature)); blob->Release(); if (FAILED(rootHr)) { ShutdownGlassEffects(); return false; }
        ID3DBlob* vs = CompileGlassShader("VSMain", "vs_5_0");
        ID3DBlob* blur = CompileGlassShader("GaussianPS", "ps_5_0");
        ID3DBlob* backdropBlur = CompileGlassShader("BackdropGaussianPS", "ps_5_0");
        ID3DBlob* background = CompileGlassShader("BackgroundPS", "ps_5_0");
        ID3DBlob* filter = CompileGlassShader("FilterPS", "ps_5_0");
        ID3DBlob* composite = CompileGlassShader("CompositePS", "ps_5_0");
        ID3DBlob* uiComposite = CompileGlassShader("UiCompositePS", "ps_5_0");
        ID3DBlob* glow = CompileGlassShader("GlowPS", "ps_5_0");
        if (!vs || !blur || !backdropBlur || !background || !filter || !composite || !uiComposite || !glow)
        {
            if (vs) vs->Release(); if (blur) blur->Release(); if (backdropBlur) backdropBlur->Release(); if (background) background->Release();
            if (filter) filter->Release();
            if (composite) composite->Release(); if (uiComposite) uiComposite->Release(); if (glow) glow->Release(); ShutdownGlassEffects(); return false;
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{}; pso.pRootSignature = g_glassEffects.rootSignature; pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() }; pso.PS = { blur->GetBufferPointer(), blur->GetBufferSize() }; pso.SampleMask = UINT_MAX;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pso.RasterizerState.DepthClipEnable = TRUE; pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; pso.NumRenderTargets = 1; pso.RTVFormats[0] = g_glassEffects.outputFormat; pso.SampleDesc.Count = 1; pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.gaussianPipeline)))) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        pso.PS = { backdropBlur->GetBufferPointer(), backdropBlur->GetBufferSize() };
        if (FAILED(g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.backdropGaussianPipeline)))) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        pso.PS = { background->GetBufferPointer(), background->GetBufferSize() };
        if (FAILED(g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.backgroundPipeline)))) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        pso.PS = { filter->GetBufferPointer(), filter->GetBufferSize() };
        if (FAILED(g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.filterPipeline)))) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        auto& blend = pso.BlendState.RenderTarget[0]; blend.BlendEnable = TRUE; blend.SrcBlend = D3D12_BLEND_ONE; blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; blend.BlendOp = D3D12_BLEND_OP_ADD; blend.SrcBlendAlpha = D3D12_BLEND_ONE; blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso.PS = { composite->GetBufferPointer(), composite->GetBufferSize() };
        const HRESULT compositeHr = g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.compositePipeline));
        if (FAILED(compositeHr)) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        pso.PS = { uiComposite->GetBufferPointer(), uiComposite->GetBufferSize() };
        if (FAILED(g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.uiCompositePipeline)))) { vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release(); ShutdownGlassEffects(); return false; }
        blend.SrcBlend = D3D12_BLEND_ONE; blend.DestBlend = D3D12_BLEND_ONE;
        blend.SrcBlendAlpha = D3D12_BLEND_ZERO; blend.DestBlendAlpha = D3D12_BLEND_ONE;
        pso.PS = { glow->GetBufferPointer(), glow->GetBufferSize() };
        const HRESULT glowHr = g_glassEffects.device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_glassEffects.glowPipeline));
        vs->Release(); blur->Release(); backdropBlur->Release(); background->Release(); filter->Release(); composite->Release(); uiComposite->Release(); glow->Release();
        if (FAILED(glowHr)) { ShutdownGlassEffects(); return false; }
        D3D12_DESCRIPTOR_HEAP_DESC srv{}; srv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; srv.NumDescriptors = 5; srv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; D3D12_DESCRIPTOR_HEAP_DESC rtv{}; rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; rtv.NumDescriptors = 5;
        if (FAILED(g_glassEffects.device->CreateDescriptorHeap(&srv, IID_PPV_ARGS(&g_glassEffects.srvHeap))) || FAILED(g_glassEffects.device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&g_glassEffects.rtvHeap)))) { ShutdownGlassEffects(); return false; }
        g_glassEffects.srvDescriptorStride = g_glassEffects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); return true;
    }
    static void RenderGaussianPass(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE source, ID3D12Resource* target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, float dx, float dy, float sigma)
    {
        Transition(cmd, target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET); cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr); SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(pipeline); cmd->SetGraphicsRootDescriptorTable(0, source);
        const IVUI::GlassShaderConstants constants = IVUI::MakeGaussianBlurConstants(ImVec2((float)g_glassEffects.outputWidth, (float)g_glassEffects.outputHeight), ImVec2(dx, dy), sigma);
        cmd->SetGraphicsRoot32BitConstants(1, 44, constants.values, 0); cmd->DrawInstanced(3, 1, 0, 0); Transition(cmd, target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    static void RenderBloomFilterPass(ID3D12GraphicsCommandList* cmd, float brightnessThreshold, float saturationThreshold)
    {
        Transition(cmd, g_glassEffects.filterTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->OMSetRenderTargets(1, &g_glassEffects.filterRtv, FALSE, nullptr);
        SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(g_glassEffects.filterPipeline);
        cmd->SetGraphicsRootDescriptorTable(0, g_glassEffects.uiSrv);
        const IVUI::GlassShaderConstants constants = IVUI::MakeBloomFilterConstants(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), brightnessThreshold, saturationThreshold);
        cmd->SetGraphicsRoot32BitConstants(1, 44, constants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
        Transition(cmd, g_glassEffects.filterTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    static void RenderBackgroundEffects(ID3D12GraphicsCommandList* cmd, ImVec2 size, float timeSeconds)
    {
        if (!cmd || size.x <= 0.0f || size.y <= 0.0f)
            return;
        Transition(cmd, g_glassEffects.backgroundTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float clear[4] = { 0, 0, 0, 1 };
        cmd->ClearRenderTargetView(g_glassEffects.backgroundRtv, clear, 0, nullptr);
        cmd->OMSetRenderTargets(1, &g_glassEffects.backgroundRtv, FALSE, nullptr);
        SetFullscreenState(cmd, (UINT)size.x, (UINT)size.y);
        cmd->SetGraphicsRootSignature(g_glassEffects.rootSignature);
        cmd->SetPipelineState(g_glassEffects.backgroundPipeline);
        const IVUI::GlassShaderConstants constants = IVUI::MakeBackgroundConstants(size, timeSeconds);
        cmd->SetGraphicsRoot32BitConstants(1, 44, constants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
        Transition(cmd, g_glassEffects.backgroundTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    static void CopyBackgroundToTarget(ID3D12GraphicsCommandList* cmd, ID3D12Resource* target)
    {
        Transition(cmd, g_glassEffects.backgroundTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(cmd, target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyResource(target, g_glassEffects.backgroundTexture);
        Transition(cmd, target, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Transition(cmd, g_glassEffects.backgroundTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    static void RenderFramebufferGlow(ID3D12GraphicsCommandList* cmd, ID3D12Resource* target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, ImVec2 size, float bloomStrength, float brightnessThreshold, float saturationThreshold, float sigma)
    {
        if (!cmd || !target || !EnsureGlassTextures((UINT)size.x, (UINT)size.y))
            return;
        ID3D12DescriptorHeap* heaps[] = { g_glassEffects.srvHeap };
        cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetGraphicsRootSignature(g_glassEffects.rootSignature);
        RenderBloomFilterPass(cmd, brightnessThreshold, saturationThreshold);
        RenderGaussianPass(cmd, g_glassEffects.gaussianPipeline, g_glassEffects.filterSrv, g_glassEffects.horizontalBlurTexture, g_glassEffects.horizontalBlurRtv, 1.0f, 0.0f, sigma);
        RenderGaussianPass(cmd, g_glassEffects.gaussianPipeline, g_glassEffects.horizontalBlurSrv, g_glassEffects.verticalBlurTexture, g_glassEffects.verticalBlurRtv, 0.0f, 1.0f, sigma);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(g_glassEffects.glowPipeline);
        cmd->SetGraphicsRootDescriptorTable(0, g_glassEffects.verticalBlurSrv);
        const IVUI::GlassShaderConstants constants = IVUI::MakeGlowCompositeConstants(bloomStrength);
        cmd->SetGraphicsRoot32BitConstants(1, 44, constants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
    }
    static void CompositeUiTexture(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv)
    {
        ID3D12DescriptorHeap* heaps[] = { g_glassEffects.srvHeap };
        cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetGraphicsRootSignature(g_glassEffects.rootSignature);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(g_glassEffects.uiCompositePipeline);
        cmd->SetGraphicsRootDescriptorTable(0, g_glassEffects.uiSrv);
        const IVUI::GlassShaderConstants constants{};
        cmd->SetGraphicsRoot32BitConstants(1, 44, constants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
    }
    struct BackdropProcessRequest
    {
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12Resource* backdrop = nullptr;
        ImVec2 size{};
        float blurSigma = 0.0f;
    };

    struct BackdropProcessOutput
    {
        ID3D12Resource* texture = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE srv{};
    };

    // Captures the host backdrop and returns the texture produced by the optional Gaussian stage.
    static BackdropProcessOutput DispatchBackdropProcess(const BackdropProcessRequest& request)
    {
        if (!request.commandList || !request.backdrop || !EnsureGlassTextures((UINT)request.size.x, (UINT)request.size.y))
            return {};

        ID3D12GraphicsCommandList* cmd = request.commandList;
        Transition(cmd, request.backdrop, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(cmd, g_glassEffects.sourceTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyResource(g_glassEffects.sourceTexture, request.backdrop);
        Transition(cmd, g_glassEffects.sourceTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        Transition(cmd, request.backdrop, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (request.blurSigma <= 0.01f)
            return { g_glassEffects.sourceTexture, g_glassEffects.sourceSrv };

        RenderGaussianPass(cmd, g_glassEffects.backdropGaussianPipeline, g_glassEffects.sourceSrv,
                           g_glassEffects.horizontalBlurTexture, g_glassEffects.horizontalBlurRtv, 1, 0, request.blurSigma);
        RenderGaussianPass(cmd, g_glassEffects.backdropGaussianPipeline, g_glassEffects.horizontalBlurSrv,
                           g_glassEffects.verticalBlurTexture, g_glassEffects.verticalBlurRtv, 0, 1, request.blurSigma);
        return { g_glassEffects.verticalBlurTexture, g_glassEffects.verticalBlurSrv };
    }

    // Renders the refracted material to an intermediate image, then composites that image into the UI target.
    static void DispatchGlassMaterial(
        ID3D12GraphicsCommandList* cmd,
        const BackdropProcessOutput& backdrop,
        ID3D12Resource* target,
        D3D12_CPU_DESCRIPTOR_HANDLE targetRtv,
        ImVec2 size,
        const IVUI::GlassRect& rect,
        float timeSeconds)
    {
        if (!cmd || !backdrop.texture || !target || !EnsureGlassTextures((UINT)size.x, (UINT)size.y))
            return;

        const float transparent[4] = { 0, 0, 0, 0 };
        Transition(cmd, g_glassEffects.filterTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ClearRenderTargetView(g_glassEffects.filterRtv, transparent, 0, nullptr);
        cmd->OMSetRenderTargets(1, &g_glassEffects.filterRtv, FALSE, nullptr);
        SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(g_glassEffects.compositePipeline);
        cmd->SetGraphicsRootDescriptorTable(0, backdrop.srv);
        const IVUI::GlassShaderConstants materialConstants = IVUI::MakeGlassCompositeConstants(rect, size, timeSeconds);
        cmd->SetGraphicsRoot32BitConstants(1, 44, materialConstants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
        Transition(cmd, g_glassEffects.filterTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        cmd->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);
        SetFullscreenState(cmd, g_glassEffects.outputWidth, g_glassEffects.outputHeight);
        cmd->SetPipelineState(g_glassEffects.uiCompositePipeline);
        cmd->SetGraphicsRootDescriptorTable(0, g_glassEffects.filterSrv);
        const IVUI::GlassShaderConstants compositeConstants{};
        cmd->SetGraphicsRoot32BitConstants(1, 44, compositeConstants.values, 0);
        cmd->DrawInstanced(3, 1, 0, 0);
    }

    struct GlassRenderContext
    {
        ID3D12GraphicsCommandList* commandList = nullptr;
        ID3D12Resource* backdrop = nullptr;
        ID3D12Resource* target = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        ImVec2 size{};
        float timeSeconds = 0.0f;
    };
    static GlassRenderContext g_glassRenderContext;

    static void RenderGlassAtWindowDepth(const IVUI::GlassRect& rect)
    {
        const GlassRenderContext& context = g_glassRenderContext;
        if (context.commandList == nullptr || context.backdrop == nullptr || context.target == nullptr)
            return;

        // The dispatcher returns an explicit backdrop image before the material pass consumes it.
        const float backdropBlurSigma = std::clamp(IVUI::GetTheme().BackdropBlurSigma, 0.0f, 16.0f);
        ID3D12DescriptorHeap* heaps[] = { g_glassEffects.srvHeap };
        context.commandList->SetDescriptorHeaps(1, heaps);
        context.commandList->SetGraphicsRootSignature(g_glassEffects.rootSignature);
        const BackdropProcessOutput backdrop = DispatchBackdropProcess({ context.commandList, context.backdrop, context.size, backdropBlurSigma });
        DispatchGlassMaterial(context.commandList, backdrop, context.target, context.rtv, context.size, rect, context.timeSeconds);
    }
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

static const int APP_NUM_FRAMES_IN_FLIGHT = 2;
static const int APP_NUM_BACK_BUFFERS = 2;
static const int APP_SRV_HEAP_SIZE = 64;

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator = nullptr;
    UINT64 FenceValue = 0;
};

struct DescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu{};
    UINT HeapHandleIncrement = 0;
    ImVector<int> FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        Heap = heap;
        auto desc = heap->GetDesc();
        HeapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(desc.Type);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; --n)
            FreeIndices.push_back(n - 1);
    }

    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }

    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        cpu->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        gpu->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        int cpu_idx = (int)((cpu.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        (void)gpu;
        FreeIndices.push_back(cpu_idx);
    }
};

static FrameContext g_frameContext[APP_NUM_FRAMES_IN_FLIGHT];
static UINT g_frameIndex = 0;
static ID3D12Device* g_pd3dDevice = nullptr;
static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
static DescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ID3D12CommandQueue* g_pd3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList* g_pd3dCommandList = nullptr;
static ID3D12Fence* g_fence = nullptr;
static HANDLE g_fenceEvent = nullptr;
static UINT64 g_fenceLastSignaledValue = 0;
static IDXGISwapChain3* g_pSwapChain = nullptr;
static HANDLE g_hSwapChainWaitableObject = nullptr;
static ID3D12Resource* g_mainRenderTargetResource[APP_NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[APP_NUM_BACK_BUFFERS] = {};
static bool g_SwapChainOccluded = false;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static void WaitForPendingOperations();
static FrameContext* WaitForNextFrameContext();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// SRV descriptor alloc/free callbacks for the 1.92 DX12 backend (needed so the
// dynamic font atlas can create/update textures without flicker).
static void SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
{
    g_pd3dSrvDescHeapAlloc.Alloc(out_cpu, out_gpu);
}
static void SrvDescriptorFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
{
    g_pd3dSrvDescHeapAlloc.Free(cpu, gpu);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Render at native pixels (no OS bitmap upscaling) so text/UI stay crisp on
    // high-DPI displays.
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"ImGuiVectorUIDemo", nullptr };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"ImGUI_Vector_UI",
                                  WS_OVERLAPPEDWINDOW, 100, 100, 1600, 900, nullptr, nullptr, wc.hInstance, nullptr);

    // Scale the initial window by the monitor DPI and center it. With DPI
    // awareness on, a fixed 1600x900 is physical pixels and looks tiny on hi-DPI
    // screens; this keeps a comfortable apparent size, clamped to the screen.
    {
        const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        int w = (int)(1600.0f * dpi);
        int h = (int)(900.0f * dpi);
        const int sx = ::GetSystemMetrics(SM_CXSCREEN);
        const int sy = ::GetSystemMetrics(SM_CYSCREEN);
        if (w > (int)(sx * 0.92f)) w = (int)(sx * 0.92f);
        if (h > (int)(sy * 0.92f)) h = (int)(sy * 0.92f);
        ::SetWindowPos(hwnd, nullptr, (sx - w) / 2, (sy - h) / 2, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
    ::SetActiveWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Clean base style: the ImGuiVectorUI library draws its own rounded surfaces
    // and borders, so strip ImGui's default window chrome (that stray non-rounded
    // backdrop) and make window/child backgrounds transparent.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);

    // Fonts: a crisp Latin UI font (Segoe UI) with bundled GNU Unifont merged in
    // as a full-Unicode fallback (CJK, Cyrillic, symbols, ...). ImGui 1.92's
    // dynamic atlas rasterizes any needed glyph on demand, so no ranges are listed.
    char exePathBuf[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    std::string exeDir(exePathBuf);
    exeDir = exeDir.substr(0, exeDir.find_last_of("\\/") + 1);
    const std::string unifontPath = exeDir + "unifont.otf";
    const bool haveUnifont = (::GetFileAttributesA(unifontPath.c_str()) != INVALID_FILE_ATTRIBUTES);

    auto mergeUnifont = [&]()
    {
        if (!haveUnifont)
            return;
        ImFontConfig cfg;
        cfg.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(unifontPath.c_str(), 16.0f, &cfg);
    };

    ImFontConfig baseCfg;
    baseCfg.OversampleH = 3;
    baseCfg.OversampleV = 1;
    baseCfg.PixelSnapH = true;

    ImFont* fontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &baseCfg);
    if (fontRegular == nullptr)
        fontRegular = io.Fonts->AddFontDefault();
    mergeUnifont();

    ImFont* fontTitle = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f, &baseCfg);
    if (fontTitle == nullptr)
        fontTitle = fontRegular;
    mergeUnifont();

    ImGui_ImplWin32_Init(hwnd);

    // Use the 1.92 InitInfo API with descriptor alloc/free callbacks so the dynamic
    // font atlas can allocate/refresh SRVs (the legacy single-descriptor init cannot,
    // which caused flicker whenever new glyphs were baked as UI values changed).
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = g_pd3dDevice;
    init_info.CommandQueue = g_pd3dCommandQueue;
    init_info.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
    init_info.SrvDescriptorAllocFn = SrvDescriptorAlloc;
    init_info.SrvDescriptorFreeFn = SrvDescriptorFree;
    ImGui_ImplDX12_Init(&init_info);

    if (!InitGlassEffects(g_pd3dDevice, DXGI_FORMAT_R8G8B8A8_UNORM))
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    IVUI::CreateContext();
    IVUI::SetFonts(fontRegular, fontTitle);
    IVUI::SetContentScale(ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd));
    IVUI::SetGlassRenderCallback(RenderGlassAtWindowDepth);

    bool show_menu = true;
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Toggle the menu on Insert/F1 — unless a hotkey field is capturing or a
        // text input is focused (so those keystrokes go to the field instead).
        const bool togglePressed = ((::GetAsyncKeyState(VK_INSERT) & 1) != 0) || ((::GetAsyncKeyState(VK_F1) & 1) != 0);
        if (togglePressed && !IVUI::WantCaptureHotkey() && !ImGui::GetIO().WantTextInput)
            show_menu = !show_menu;

        if ((g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) || ::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        IVUI::NewFrame();

        // The menu owns its own show/hide fade + scrim, so render it every frame.
        Demo::RenderMenu(&show_menu);

        ImGui::Render();

        FrameContext* frameCtx = WaitForNextFrameContext();
        UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
        frameCtx->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clearColor, 0, nullptr);
        g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        g_pSwapChain->GetDesc1(&scd);
        const ImVec2 renderTargetSize((float)scd.Width, (float)scd.Height);
        const bool effectsReady = EnsureGlassTextures((UINT)renderTargetSize.x, (UINT)renderTargetSize.y);
        if (effectsReady)
        {
            RenderBackgroundEffects(g_pd3dCommandList, renderTargetSize, (float)ImGui::GetTime());
            CopyBackgroundToTarget(g_pd3dCommandList, g_mainRenderTargetResource[backBufferIdx]);
            Transition(g_pd3dCommandList, g_glassEffects.uiTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            const float transparent[4] = { 0, 0, 0, 0 };
            g_pd3dCommandList->ClearRenderTargetView(g_glassEffects.uiRtv, transparent, 0, nullptr);
            g_glassRenderContext = { g_pd3dCommandList, g_glassEffects.backgroundTexture,
                                     g_glassEffects.uiTexture, g_glassEffects.uiRtv, renderTargetSize, (float)ImGui::GetTime() };
            g_pd3dCommandList->OMSetRenderTargets(1, &g_glassEffects.uiRtv, FALSE, nullptr);
            g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
            g_glassRenderContext = {};
            Transition(g_pd3dCommandList, g_glassEffects.uiTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            CompositeUiTexture(g_pd3dCommandList, g_mainRenderTargetDescriptor[backBufferIdx]);
            const IVUI::Theme& theme = IVUI::GetTheme();
            const float bloomStrength = std::clamp(theme.PanelGlow * 1.7f, 0.0f, 1.5f);
            RenderFramebufferGlow(g_pd3dCommandList, g_mainRenderTargetResource[backBufferIdx],
                g_mainRenderTargetDescriptor[backBufferIdx], renderTargetSize, bloomStrength,
                std::clamp(theme.BloomThreshold, 0.0f, 1.0f), std::clamp(theme.BloomSaturation, 0.0f, 1.0f),
                std::clamp(theme.BloomSigma, 0.5f, 3.0f));
        }
        else
        {
            g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
            g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
        }
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->Close();

        g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);
        g_pd3dCommandQueue->Signal(g_fence, ++g_fenceLastSignaledValue);
        frameCtx->FenceValue = g_fenceLastSignaledValue;

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
        g_frameIndex++;
    }

    WaitForPendingOperations();
    IVUI::SetGlassRenderCallback(nullptr);
    ShutdownGlassEffects();
    IVUI::DestroyContext();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

static bool CreateDeviceD3D(HWND hWnd)
{
    UINT dxgiFactoryFlags = 0;
#ifdef DX12_ENABLE_DEBUG_LAYER
    {
        ID3D12Debug* debug = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
    }
#endif

    IDXGIFactory4* dxgiFactory = nullptr;
    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory))))
        return false;

    IDXGIAdapter1* adapter = nullptr;
    for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pd3dDevice))))
            break;
    }
    if (!g_pd3dDevice)
        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pd3dDevice));

    if (!g_pd3dDevice)
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = APP_SRV_HEAP_SIZE;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_pd3dSrvDescHeap))))
        return false;
    g_pd3dSrvDescHeapAlloc.Create(g_pd3dDevice, g_pd3dSrvDescHeap);

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = APP_NUM_BACK_BUFFERS;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_pd3dRtvDescHeap))))
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(g_pd3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_pd3dCommandQueue))))
        return false;

    for (int i = 0; i < APP_NUM_FRAMES_IN_FLIGHT; ++i)
        if (FAILED(g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator))))
            return false;

    if (FAILED(g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&g_pd3dCommandList))))
        return false;
    g_pd3dCommandList->Close();

    if (FAILED(g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))))
        return false;
    g_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent)
        return false;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = APP_NUM_BACK_BUFFERS;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;
    // Required for SetMaximumFrameLatency + GetFrameLatencyWaitableObject to work.
    // Without it the waitable handle is invalid, frame pacing breaks, and fine
    // detail (text, thin icons) tears/flickers under rapid updates.
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain1* swapChain1 = nullptr;

    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1)))
    {
        dxgiFactory->Release();
        return false;
    }

    if (FAILED(swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain))))
    {
        swapChain1->Release();
        dxgiFactory->Release();
        return false;
    }
    swapChain1->Release();
    dxgiFactory->Release();

    g_pSwapChain->SetMaximumFrameLatency(APP_NUM_BACK_BUFFERS);
    g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->SetFullscreenState(false, nullptr); g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_hSwapChainWaitableObject) { CloseHandle(g_hSwapChainWaitableObject); g_hSwapChainWaitableObject = nullptr; }
    for (UINT i = 0; i < APP_NUM_FRAMES_IN_FLIGHT; ++i)
        if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = nullptr; }
    if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = nullptr; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = nullptr; }
    if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = nullptr; }
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = nullptr; }
    if (g_fence) { g_fence->Release(); g_fence = nullptr; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

static void CreateRenderTarget()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT rtvIncrement = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; ++i)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_mainRenderTargetDescriptor[i] = rtvHandle;
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
        rtvHandle.ptr += rtvIncrement;
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

static void CleanupRenderTarget()
{
    WaitForPendingOperations();
    for (UINT i = 0; i < APP_NUM_BACK_BUFFERS; ++i)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = nullptr; }
}

static void WaitForPendingOperations()
{
    g_pd3dCommandQueue->Signal(g_fence, ++g_fenceLastSignaledValue);
    g_fence->SetEventOnCompletion(g_fenceLastSignaledValue, g_fenceEvent);
    ::WaitForSingleObject(g_fenceEvent, INFINITE);
}

static FrameContext* WaitForNextFrameContext()
{
    FrameContext* frame_context = &g_frameContext[g_frameIndex % APP_NUM_FRAMES_IN_FLIGHT];
    if (g_fence->GetCompletedValue() < frame_context->FenceValue)
    {
        g_fence->SetEventOnCompletion(frame_context->FenceValue, g_fenceEvent);
        HANDLE waitableObjects[] = { g_hSwapChainWaitableObject, g_fenceEvent };
        ::WaitForMultipleObjects(2, waitableObjects, TRUE, INFINITE);
    }
    else
        ::WaitForSingleObject(g_hSwapChainWaitableObject, INFINITE);
    return frame_context;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_ERASEBKGND:
        // We render the whole client via DX12; let GDI erase nothing so there is
        // no background flash between GDI and the swapchain present.
        return 1;
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN,
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
