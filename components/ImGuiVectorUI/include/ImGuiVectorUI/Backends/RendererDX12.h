/** @file RendererDX12.h @brief Dear ImGui Direct3D 12 renderer adapter. */
#pragma once

#include "ImGuiVectorUI/UI.h"
#include "ImGuiVectorUI/Backends/EffectsDX12.h"

namespace IVUI::DX12
{
    using AllocateSrv = void (*)(void* userData, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                 D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    using FreeSrv = void (*)(void* userData, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                            D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    struct RendererCreateInfo
    {
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* commandQueue = nullptr;
        int framesInFlight = 2;
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
        ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
        AllocateSrv allocateSrv = nullptr;
        FreeSrv freeSrv = nullptr;
        void* userData = nullptr;
    };

    struct RenderFrame final : IVUI::RenderFrame
    {
        EffectRendererFrameInfo effects{};
        ID3D12DescriptorHeap* const* descriptorHeaps = nullptr;
        UINT descriptorHeapCount = 0;
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
