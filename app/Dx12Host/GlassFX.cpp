#include "GlassFX.h"

#include "ImGuiVectorUI/Drawing/Effects.h"

#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace GlassFX
{
    static ID3D12RootSignature* g_rootSig = nullptr;
    static ID3D12PipelineState* g_pso = nullptr;

    // Fullscreen-triangle VS + rounded-box SDF composite PS. The cbuffer is a flat
    // float4[6] (24 root constants) to avoid HLSL cbuffer packing surprises.
    static const char* kShaderSource = R"(
cbuffer C : register(b0) { float4 uData[6]; };
#define RECTMIN  uData[0].xy
#define RECTMAX  uData[0].zw
#define ROUND    uData[1].z
#define OPACITY  uData[1].w
#define TINT     uData[2].rgb
#define ACCENT   uData[3].rgb
#define GLOW     uData[4].rgb
#define FX       uData[5]   // x=glowStrength y=glowSpread z=shadowStrength w=shadowSpread

struct VSOut { float4 pos : SV_Position; };

VSOut VSMain(uint vid : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2); // (0,0) (2,0) (0,2)
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);    // covers the whole screen
    return o;
}

float RoundedBoxSdf(float2 p, float2 c, float2 h, float r)
{
    float2 q = abs(p - c) - h + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// premultiplied "over": composite `top` (straight color + alpha) onto premultiplied `acc`.
float4 Over(float4 acc, float3 c, float a)
{
    a = saturate(a);
    acc.rgb = c * a + acc.rgb * (1.0 - a);
    acc.a   = a + acc.a * (1.0 - a);
    return acc;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 px     = i.pos.xy;
    float2 center = (RECTMIN + RECTMAX) * 0.5;
    float2 half   = (RECTMAX - RECTMIN) * 0.5;

    float sdf     = RoundedBoxSdf(px, center, half, ROUND);
    float mask    = saturate(1.0 - smoothstep(0.0, 1.5, sdf));
    float border  = saturate(1.0 - smoothstep(1.0, 3.5, abs(sdf)));
    float outside = max(sdf, 0.0);

    float glowA   = saturate(exp(-outside / max(FX.y, 0.001)) * FX.x * (1.0 - mask));

    float2 shCenter = center + float2(0.0, 10.0);
    float shSdf     = RoundedBoxSdf(px, shCenter, half, ROUND);
    float shadowA   = saturate(exp(-max(shSdf, 0.0) / max(FX.w, 0.001)) * FX.z * (1.0 - mask));

    float3 glass = TINT + ACCENT * (0.08 + border * 0.28);
    float fillA  = saturate(mask * OPACITY);

    float4 acc = float4(0.0, 0.0, 0.0, 0.0); // premultiplied
    acc = Over(acc, float3(0.0, 0.0, 0.0), shadowA); // drop shadow (dark)
    acc = Over(acc, GLOW,  glowA);                   // outer glow
    acc = Over(acc, glass, fillA);                   // glass body
    return acc;                                      // already premultiplied
}
)";

    static ID3DBlob* Compile(const char* entry, const char* target)
    {
        ID3DBlob* code = nullptr;
        ID3DBlob* err = nullptr;
        UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS;
        HRESULT hr = D3DCompile(kShaderSource, strlen(kShaderSource), "GlassFX", nullptr, nullptr,
                                entry, target, flags, 0, &code, &err);
        if (err) err->Release();
        if (FAILED(hr))
        {
            if (code) code->Release();
            return nullptr;
        }
        return code;
    }

    bool Init(ID3D12Device* device, DXGI_FORMAT rtvFormat)
    {
        if (!device)
            return false;

        // Root signature: a single set of 24 root 32-bit constants at b0 (PS only).
        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        param.Constants.ShaderRegister = 0;
        param.Constants.RegisterSpace = 0;
        param.Constants.Num32BitValues = 24;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters = 1;
        rsDesc.pParameters = &param;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* rsBlob = nullptr;
        ID3DBlob* rsErr = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErr)))
        {
            if (rsErr) rsErr->Release();
            if (rsBlob) rsBlob->Release();
            return false;
        }
        HRESULT hr = device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(),
                                                 IID_PPV_ARGS(&g_rootSig));
        rsBlob->Release();
        if (rsErr) rsErr->Release();
        if (FAILED(hr))
            return false;

        ID3DBlob* vs = Compile("VSMain", "vs_5_0");
        ID3DBlob* ps = Compile("PSMain", "ps_5_0");
        if (!vs || !ps)
        {
            if (vs) vs->Release();
            if (ps) ps->Release();
            Shutdown();
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = g_rootSig;
        pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        pso.SampleMask = 0xFFFFFFFFu;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = rtvFormat;
        pso.SampleDesc.Count = 1;

        // Rasterizer: no cull, solid.
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;

        // Premultiplied "over" blend: Src=ONE, Dest=INV_SRC_ALPHA (color & alpha).
        D3D12_RENDER_TARGET_BLEND_DESC& rt = pso.BlendState.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_ONE;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Depth/stencil off.
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;

        hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_pso));
        vs->Release();
        ps->Release();
        if (FAILED(hr))
        {
            Shutdown();
            return false;
        }
        return true;
    }

    static inline void UnpackRGB(ImU32 col, float* out)
    {
        out[0] = ((col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        out[1] = ((col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        out[2] = ((col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    }

    void Render(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv, ImVec2 viewportPx)
    {
        if (!g_pso || !g_rootSig || !cmd)
            return;

        int count = 0;
        const IVUI::GlassRect* rects = IVUI::CollectGlass(&count);
        if (count <= 0 || !rects)
            return;

        D3D12_VIEWPORT vp = { 0.0f, 0.0f, viewportPx.x, viewportPx.y, 0.0f, 1.0f };
        D3D12_RECT sc = { 0, 0, (LONG)viewportPx.x, (LONG)viewportPx.y };
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd->SetGraphicsRootSignature(g_rootSig);
        cmd->SetPipelineState(g_pso);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (int i = 0; i < count; ++i)
        {
            const IVUI::GlassRect& r = rects[i];
            float d[24] = {};
            d[0] = r.min.x; d[1] = r.min.y; d[2] = r.max.x; d[3] = r.max.y;
            d[4] = viewportPx.x; d[5] = viewportPx.y; d[6] = r.rounding; d[7] = r.opacity;
            UnpackRGB(r.tint, &d[8]);   d[11] = 0.0f;
            UnpackRGB(r.accent, &d[12]); d[15] = 0.0f;
            UnpackRGB(r.glow, &d[16]);   d[19] = 0.0f;
            d[20] = r.glowStrength; d[21] = r.glowSpread; d[22] = r.shadowStrength; d[23] = r.shadowSpread;
            cmd->SetGraphicsRoot32BitConstants(0, 24, d, 0);
            cmd->DrawInstanced(3, 1, 0, 0);
        }
    }

    void Shutdown()
    {
        if (g_pso) { g_pso->Release(); g_pso = nullptr; }
        if (g_rootSig) { g_rootSig->Release(); g_rootSig = nullptr; }
    }
}
