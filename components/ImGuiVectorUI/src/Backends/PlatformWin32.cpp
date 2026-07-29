#include "ImGuiVectorUI/Backends/PlatformWin32.h"

#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace IVUI::Win32
{
    bool PlatformBackend::Initialize(ImGuiContext&, std::string& error) noexcept
    {
        if (window_ == nullptr || !ImGui_ImplWin32_Init(window_))
        {
            error = "Win32 platform initialization failed.";
            return false;
        }
        initialized_ = true;
        return true;
    }

    void PlatformBackend::Shutdown() noexcept
    {
        if (initialized_)
            ImGui_ImplWin32_Shutdown();
        initialized_ = false;
    }

    void PlatformBackend::NewFrame() noexcept
    {
        if (initialized_)
            ImGui_ImplWin32_NewFrame();
    }

    bool PlatformBackend::ProcessEvent(const PlatformEvent& event) noexcept
    {
        if (!initialized_ || event.native == nullptr)
            return false;
        const auto& native = *static_cast<const Event*>(event.native);
        return ImGui_ImplWin32_WndProcHandler(native.window, native.message,
                                               native.wParam, native.lParam) != 0;
    }

    float PlatformBackend::GetContentScale() const noexcept
    {
        return window_ ? ImGui_ImplWin32_GetDpiScaleForHwnd(window_) : 1.0f;
    }
}
