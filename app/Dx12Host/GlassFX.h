// GlassFX — a small custom DX12 shader pass that renders the IVUI "glass" queue.
//
// Ported/adapted from BL4_Hack's BackdropBlur SDF composite (Interface/Overlay),
// stripped of the scene-copy / mip-blur passes: it draws, per queued IVUI::GlassRect,
// a rounded-box SDF panel with translucent tint + outer glow + drop shadow + edge
// sheen, in premultiplied alpha so it composites correctly onto a transparent
// swapchain. Runs once per frame, BEFORE ImGui_ImplDX12_RenderDrawData, so panel
// glass sits beneath the ImGui-drawn controls and text.
#pragma once

#include "imgui.h"
#include <d3d12.h>

namespace GlassFX
{
    // Create root signature + PSO (runtime-compiles the embedded HLSL). rtvFormat
    // must match the swapchain's back-buffer format. Returns false on failure
    // (host should skip the pass but keep running).
    bool Init(ID3D12Device* device, DXGI_FORMAT rtvFormat);

    // Draw every IVUI::CollectGlass() rect into `rtv`. `viewportPx` is the back
    // buffer size in pixels. No-op if not initialized or the queue is empty.
    void Render(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv, ImVec2 viewportPx);

    void Shutdown();
}
