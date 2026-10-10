#include "ImGuiVectorUI/Backends/RendererDX12.h"

#include "imgui_impl_dx12.h"

namespace IVUI::DX12
{
    namespace
    {
        void Allocate(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                      D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
        {
            auto* createInfo = static_cast<RendererCreateInfo*>(info->UserData);
            createInfo->allocateSrv(createInfo->userData, cpu, gpu);
        }

        void Free(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                  D3D12_GPU_DESCRIPTOR_HANDLE gpu)
        {
            auto* createInfo = static_cast<RendererCreateInfo*>(info->UserData);
            createInfo->freeSrv(createInfo->userData, cpu, gpu);
        }
    }

    bool RendererBackend::Initialize(ImGuiContext&, std::string& error) noexcept
    {
        if (!info_.device || !info_.commandQueue || !info_.srvDescriptorHeap ||
            !info_.allocateSrv || !info_.freeSrv || info_.framesInFlight <= 0)
        {
            error = "DX12 renderer create info is incomplete.";
            return false;
        }
        ImGui_ImplDX12_InitInfo info{};
        info.Device = info_.device;
        info.CommandQueue = info_.commandQueue;
        info.NumFramesInFlight = info_.framesInFlight;
        info.RTVFormat = info_.rtvFormat;
        info.DSVFormat = info_.dsvFormat;
        info.UserData = &info_;
        info.SrvDescriptorHeap = info_.srvDescriptorHeap;
        info.SrvDescriptorAllocFn = Allocate;
        info.SrvDescriptorFreeFn = Free;
        if (!ImGui_ImplDX12_Init(&info))
        {
            error = "DX12 renderer initialization failed.";
            return false;
        }
        initialized_ = true;
        return true;
    }

    void RendererBackend::Shutdown() noexcept
    {
        if (initialized_) ImGui_ImplDX12_Shutdown();
        initialized_ = false;
    }

    void RendererBackend::NewFrame() noexcept
    {
        if (initialized_) ImGui_ImplDX12_NewFrame();
    }

    bool RendererBackend::RenderDrawData(ImDrawData& drawData, const IVUI::RenderFrame& base,
                                         IEffectRenderer* effects, std::string& error) noexcept
    {
        const auto* frame = dynamic_cast<const RenderFrame*>(&base);
        if (!initialized_ || !frame || !frame->effects.commandList)
        {
            error = "DX12 renderer received an incompatible frame.";
            return false;
        }
        auto* effectRenderer = dynamic_cast<EffectRenderer*>(effects ? effects->GetBackendRenderer() : nullptr);
        EffectRendererFrameInfo effectFrame = frame->effects;
        effectFrame.drawData = &drawData;
        if (!effectFrame.restoreSrvDescriptorHeap && frame->descriptorHeaps)
        {
            for (UINT index = 0; index < frame->descriptorHeapCount; ++index)
            {
                ID3D12DescriptorHeap* heap = frame->descriptorHeaps[index];
                if (heap && heap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
                {
                    effectFrame.restoreSrvDescriptorHeap = heap;
                    break;
                }
            }
        }
        if (!effectFrame.restoreSrvDescriptorHeap)
            effectFrame.restoreSrvDescriptorHeap = info_.srvDescriptorHeap;
        if (effectRenderer && !effectRenderer->BeginFrame(effectFrame))
        {
            error = effectRenderer->GetLastError();
            return false;
        }
        if (frame->descriptorHeaps && frame->descriptorHeapCount)
            frame->effects.commandList->SetDescriptorHeaps(frame->descriptorHeapCount,
                                                            frame->descriptorHeaps);
        ImGui_ImplDX12_RenderDrawData(&drawData, frame->effects.commandList);
        if (effectRenderer) effectRenderer->EndFrame();
        return true;
    }
}
