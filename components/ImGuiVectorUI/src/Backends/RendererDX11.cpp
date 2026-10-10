#include "ImGuiVectorUI/Backends/RendererDX11.h"

#include "imgui_impl_dx11.h"

namespace IVUI::DX11
{
    bool RendererBackend::Initialize(ImGuiContext&, std::string& error) noexcept
    {
        if (!info_.device || !info_.deviceContext ||
            !ImGui_ImplDX11_Init(info_.device, info_.deviceContext))
        {
            error = "DX11 renderer initialization failed.";
            return false;
        }
        initialized_ = true;
        return true;
    }

    void RendererBackend::Shutdown() noexcept
    {
        if (initialized_) ImGui_ImplDX11_Shutdown();
        initialized_ = false;
    }

    void RendererBackend::NewFrame() noexcept
    {
        if (initialized_) ImGui_ImplDX11_NewFrame();
    }

    bool RendererBackend::RenderDrawData(ImDrawData& drawData, const IVUI::RenderFrame& base,
                                         IEffectRenderer* effects, std::string& error) noexcept
    {
        const auto* frame = dynamic_cast<const RenderFrame*>(&base);
        if (!initialized_ || !frame)
        {
            error = "DX11 renderer received an incompatible frame.";
            return false;
        }
        auto* effectRenderer = dynamic_cast<EffectRenderer*>(effects ? effects->GetBackendRenderer() : nullptr);
        EffectRendererFrameInfo effectFrame = frame->effects;
        effectFrame.drawData = &drawData;
        if (effectRenderer && !effectRenderer->BeginFrame(effectFrame))
        {
            error = effectRenderer->GetLastError();
            return false;
        }
        ImGui_ImplDX11_RenderDrawData(&drawData);
        if (effectRenderer) effectRenderer->EndFrame();
        return true;
    }
}
