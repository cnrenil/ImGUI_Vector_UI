/** @file PlatformWin32.h @brief Win32 platform adapter owned by IVUI::UI. */
#pragma once

#include "ImGuiVectorUI/UI.h"

#include <windows.h>

namespace IVUI::Win32
{
    struct Event
    {
        HWND window = nullptr;
        UINT message = 0;
        WPARAM wParam = 0;
        LPARAM lParam = 0;
    };

    inline PlatformEvent MakeEvent(const Event& event) noexcept { return {&event}; }

    class PlatformBackend final : public IPlatformBackend
    {
    public:
        explicit PlatformBackend(HWND window) noexcept : window_(window) {}
        bool Initialize(ImGuiContext& context, std::string& error) noexcept override;
        void Shutdown() noexcept override;
        void NewFrame() noexcept override;
        bool ProcessEvent(const PlatformEvent& event) noexcept override;
        float GetContentScale() const noexcept;

    private:
        HWND window_ = nullptr;
        bool initialized_ = false;
    };
}
