// ImGuiVectorUI demonstration host using Win32 and Direct3D 11.

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "ImGuiVectorUI/ImGuiVectorUI.h"
#include "ImGuiVectorUI/Backends/EffectsDX11.h"
#include "ImGuiVectorUI/Backends/PlatformWin32.h"
#include "ImGuiVectorUI/Backends/RendererDX11.h"
#include "DemoMenu.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#include <iterator>
#include <memory>
#include <string>

namespace
{
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_deviceContext = nullptr;
    IDXGISwapChain1* g_swapChain = nullptr;
    ID3D11Texture2D* g_backBuffer = nullptr;
    ID3D11RenderTargetView* g_renderTargetView = nullptr;
    UINT g_resizeWidth = 0;
    UINT g_resizeHeight = 0;
    bool g_swapChainOccluded = false;
    std::unique_ptr<IVUI::UI> g_ui;

    bool CreateDevice(HWND hwnd);
    bool CreateRenderTarget();
    void CleanupRenderTarget();
    void CleanupDevice();
    LRESULT WINAPI WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    std::string ExecutableDirectory()
    {
        char path[MAX_PATH]{};
        ::GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string result(path);
        return result.substr(0, result.find_last_of("\\/") + 1);
    }

    void ConfigureFonts(const std::string& unifontPath, bool haveUnifont,
                        const std::shared_ptr<bool>& unifontLoaded, ImGuiIO& io,
                        ImFont*& regular, ImFont*& title)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.WindowPadding = {};
        style.Colors[ImGuiCol_WindowBg] = style.Colors[ImGuiCol_ChildBg] =
            style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);

        auto mergeUnifont = [&](float size) {
            if (!haveUnifont)
                return false;
            ImFontConfig config;
            config.MergeMode = true;
            config.PixelSnapH = true;
            return io.Fonts->AddFontFromFileTTF(
                       unifontPath.c_str(), size, &config,
                       io.Fonts->GetGlyphRangesChineseFull()) != nullptr;
        };
        ImFontConfig base;
        base.OversampleH = 3;
        base.PixelSnapH = true;
        regular = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &base);
        if (!regular)
            regular = io.Fonts->AddFontDefault();
        const bool regularMerged = mergeUnifont(18.0f);
        title = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f, &base);
        if (!title)
            title = regular;
        const bool titleMerged = mergeUnifont(20.0f);
        *unifontLoaded = regularMerged && titleMerged;
    }
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW windowClass = {
        sizeof(windowClass), CS_CLASSDC, WndProc, 0L, 0L, instance, nullptr, nullptr,
        nullptr, nullptr, L"ImGuiVectorUIDemoDX11", nullptr};
    ::RegisterClassExW(&windowClass);
    HWND hwnd = ::CreateWindowExW(
        0, windowClass.lpszClassName, L"ImGUI_Vector_UI - DirectX 11",
        WS_OVERLAPPEDWINDOW, 100, 100, 1600, 900, nullptr, nullptr,
        windowClass.hInstance, nullptr);

    const float initialDpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    int width = static_cast<int>(1600.0f * initialDpi);
    int height = static_cast<int>(900.0f * initialDpi);
    const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
    width = (width > static_cast<int>(screenWidth * 0.92f))
        ? static_cast<int>(screenWidth * 0.92f) : width;
    height = (height > static_cast<int>(screenHeight * 0.92f))
        ? static_cast<int>(screenHeight * 0.92f) : height;
    ::SetWindowPos(hwnd, nullptr, (screenWidth - width) / 2, (screenHeight - height) / 2,
                   width, height, SWP_NOZORDER | SWP_NOACTIVATE);

    if (!CreateDevice(hwnd))
    {
        ::MessageBoxA(hwnd, "Direct3D 11 initialization failed.", "ImGuiVectorUI DX11",
                      MB_OK | MB_ICONERROR);
        CleanupDevice();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);

    IMGUI_CHECKVERSION();
    std::string effectError;
    const std::string executableDirectory = ExecutableDirectory();
    auto effects = IVUI::DX11::EffectRenderer::Create({g_device}, &effectError);
    if (!effects)
    {
        ::MessageBoxA(hwnd, effectError.c_str(), "ImGuiVectorUI DX11 Effects",
                      MB_OK | MB_ICONERROR);
        CleanupDevice();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    const std::string unifontPath = executableDirectory + "unifont.otf";
    const bool haveUnifont =
        ::GetFileAttributesA(unifontPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    const auto unifontLoaded = std::make_shared<bool>(false);

    IVUI::UIConfig config;
    config.platform = std::make_unique<IVUI::Win32::PlatformBackend>(hwnd);
    config.renderer = std::make_unique<IVUI::DX11::RendererBackend>(
        IVUI::DX11::RendererCreateInfo{g_device, g_deviceContext});
    config.effects = std::move(effects);
    config.contentScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    config.configureFonts = [unifontPath, haveUnifont, unifontLoaded](
                                ImGuiIO& io, ImFont*& regular, ImFont*& title) {
        ConfigureFonts(unifontPath, haveUnifont, unifontLoaded, io, regular, title);
    };

    std::string uiError;
    g_ui = IVUI::UI::Create(std::move(config), &uiError);
    if (!g_ui || !*unifontLoaded)
    {
        const std::string message = !g_ui
            ? (uiError.empty() ? "IVUI initialization failed." : uiError)
            : (haveUnifont ? "Unifont could not be parsed or merged into the font atlas."
                           : "unifont.otf was not found beside ImGuiVectorUIDemoDX11.exe.");
        ::MessageBoxA(hwnd, message.c_str(), "ImGuiVectorUI font initialization",
                      MB_OK | MB_ICONERROR);
        g_ui.reset();
        CleanupDevice();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    bool showMenu = true;
    bool done = false;
    while (!done)
    {
        MSG message;
        while (::PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessage(&message);
            if (message.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_resizeWidth != 0 && g_resizeHeight != 0)
        {
            g_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
            g_deviceContext->Flush();
            CleanupRenderTarget();
            const HRESULT resizeResult = g_swapChain->ResizeBuffers(
                0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeWidth = g_resizeHeight = 0;
            if (FAILED(resizeResult) || !CreateRenderTarget())
            {
                ::MessageBoxA(hwnd, "Resizing the DX11 swap chain failed.",
                              "ImGuiVectorUI DX11", MB_OK | MB_ICONERROR);
                break;
            }
        }

        if ((g_swapChainOccluded &&
             g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) ||
            ::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }
        g_swapChainOccluded = false;

        const bool togglePressed = ((::GetAsyncKeyState(VK_INSERT) & 1) != 0) ||
                                   ((::GetAsyncKeyState(VK_F1) & 1) != 0);
        if (togglePressed && !g_ui->WantCaptureHotkey() && !g_ui->WantTextInput())
            showMenu = !showMenu;

        if (!g_ui->BeginFrame({ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd)}, &uiError))
        {
            ::MessageBoxA(hwnd, uiError.c_str(), "ImGuiVectorUI DX11", MB_OK | MB_ICONERROR);
            break;
        }
        Demo::RenderMenu(*g_ui, &showMenu);

        D3D11_TEXTURE2D_DESC targetDescription{};
        g_backBuffer->GetDesc(&targetDescription);
        const float clearColor[4] = {0.008f, 0.012f, 0.018f, 1.0f};
        g_deviceContext->OMSetRenderTargets(1, &g_renderTargetView, nullptr);
        g_deviceContext->ClearRenderTargetView(g_renderTargetView, clearColor);

        IVUI::DX11::RenderFrame frame;
        frame.effects.deviceContext = g_deviceContext;
        frame.effects.target = g_backBuffer;
        frame.effects.targetRtv = g_renderTargetView;
        frame.effects.extent = {targetDescription.Width, targetDescription.Height};
        frame.effects.timeSeconds = static_cast<float>(ImGui::GetTime());
        const IVUI::Theme& theme = g_ui->theme().Current();
        const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(theme.Accent);
        frame.effects.renderBackground = true;
        frame.effects.background.accent = {accent.x, accent.y, accent.z, accent.w};
        frame.effects.background.gridSize = 48.0f * theme.Scale;
        frame.effects.background.animationSpeed = 1.15f;
        frame.effects.background.intensity = 1.3f;
        if (!g_ui->Render(frame, &uiError))
        {
            ::MessageBoxA(hwnd, uiError.c_str(), "ImGuiVectorUI DX11 render error",
                          MB_OK | MB_ICONERROR);
            break;
        }

        const HRESULT presentResult = g_swapChain->Present(1, 0);
        g_swapChainOccluded = presentResult == DXGI_STATUS_OCCLUDED;
        if (FAILED(presentResult) && presentResult != DXGI_STATUS_OCCLUDED)
            break;
    }

    g_ui.reset();
    CleanupDevice();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return 0;
}

namespace
{
    bool CreateDevice(HWND hwnd)
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL selectedFeatureLevel{};
        HRESULT result = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
            static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION, &g_device,
            &selectedFeatureLevel, &g_deviceContext);
#ifdef _DEBUG
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            result = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
                static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
                &g_device, &selectedFeatureLevel, &g_deviceContext);
        }
#endif
        if (result == E_INVALIDARG)
            result = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels + 1,
                static_cast<UINT>(std::size(featureLevels) - 1), D3D11_SDK_VERSION,
                &g_device, &selectedFeatureLevel, &g_deviceContext);
#ifdef _DEBUG
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            result = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels + 1,
                static_cast<UINT>(std::size(featureLevels) - 1), D3D11_SDK_VERSION,
                &g_device, &selectedFeatureLevel, &g_deviceContext);
        }
#endif
        if (FAILED(result))
            result = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, featureLevels + 1,
                static_cast<UINT>(std::size(featureLevels) - 1), D3D11_SDK_VERSION,
                &g_device, &selectedFeatureLevel, &g_deviceContext);
        if (FAILED(result))
            return false;

        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory = nullptr;
        result = g_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        if (SUCCEEDED(result))
            result = dxgiDevice->GetAdapter(&adapter);
        if (SUCCEEDED(result))
            result = adapter->GetParent(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 description{};
        description.BufferCount = 2;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.SampleDesc.Count = 1;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (SUCCEEDED(result))
            result = factory->CreateSwapChainForHwnd(
                g_device, hwnd, &description, nullptr, nullptr, &g_swapChain);
        if (factory)
            factory->Release();
        if (adapter)
            adapter->Release();
        if (dxgiDevice)
            dxgiDevice->Release();
        return SUCCEEDED(result) && CreateRenderTarget();
    }

    bool CreateRenderTarget()
    {
        if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&g_backBuffer))))
            return false;
        if (FAILED(g_device->CreateRenderTargetView(
                g_backBuffer, nullptr, &g_renderTargetView)))
        {
            g_backBuffer->Release();
            g_backBuffer = nullptr;
            return false;
        }
        return true;
    }

    void CleanupRenderTarget()
    {
        if (g_renderTargetView)
        {
            g_renderTargetView->Release();
            g_renderTargetView = nullptr;
        }
        if (g_backBuffer)
        {
            g_backBuffer->Release();
            g_backBuffer = nullptr;
        }
    }

    void CleanupDevice()
    {
        CleanupRenderTarget();
        if (g_swapChain)
        {
            g_swapChain->SetFullscreenState(FALSE, nullptr);
            g_swapChain->Release();
            g_swapChain = nullptr;
        }
        if (g_deviceContext)
        {
            g_deviceContext->ClearState();
            g_deviceContext->Flush();
            g_deviceContext->Release();
            g_deviceContext = nullptr;
        }
        if (g_device)
        {
            g_device->Release();
            g_device = nullptr;
        }
    }

    LRESULT WINAPI WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (g_ui)
        {
            const IVUI::Win32::Event event{hwnd, message, wParam, lParam};
            if (g_ui->ProcessEvent(IVUI::Win32::MakeEvent(event)))
                return true;
        }

        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (g_device && wParam != SIZE_MINIMIZED)
            {
                g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
                g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
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
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
