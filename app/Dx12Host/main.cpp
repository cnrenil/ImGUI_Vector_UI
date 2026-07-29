// Dear ImGui + Win32 + DirectX 12 host for a menu-like overlay demo.

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include "ImGuiVectorUI/ImGuiVectorUI.h"
#include "ImGuiVectorUI/Backends/EffectsDX12.h"
#include "ImGuiVectorUI/Backends/PlatformWin32.h"
#include "ImGuiVectorUI/Backends/RendererDX12.h"
#include "DemoMenu.h"

#include <d3d12.h>
#include <dxgi1_5.h>
#include <tchar.h>
#include <algorithm>
#include <memory>
#include <string>

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
static std::unique_ptr<IVUI::UI> g_ui;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static void WaitForPendingOperations();
static FrameContext* WaitForNextFrameContext();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// SRV descriptor alloc/free callbacks for the 1.92 DX12 backend (needed so the
// dynamic font atlas can create/update textures without flicker).
static void UiSrvDescriptorAlloc(void*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
    g_pd3dSrvDescHeapAlloc.Alloc(cpu, gpu);
}

static void UiSrvDescriptorFree(void*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
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
    char exePathBuf[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    std::string exeDir(exePathBuf);
    exeDir = exeDir.substr(0, exeDir.find_last_of("\\/") + 1);
    const std::string unifontPath = exeDir + "unifont.otf";
    const bool haveUnifont = (::GetFileAttributesA(unifontPath.c_str()) != INVALID_FILE_ATTRIBUTES);

    std::string effectError;
    auto effects = IVUI::DX12::EffectRenderer::Create(
        {g_pd3dDevice, DXGI_FORMAT_R8G8B8A8_UNORM}, &effectError);
    if (!effects)
    {
        ::MessageBoxA(hwnd, effectError.c_str(), "IVUI DX12 Effects", MB_OK | MB_ICONERROR);
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    IVUI::UIConfig uiConfig;
    uiConfig.platform = std::make_unique<IVUI::Win32::PlatformBackend>(hwnd);
    uiConfig.renderer = std::make_unique<IVUI::DX12::RendererBackend>(
        IVUI::DX12::RendererCreateInfo{g_pd3dDevice, g_pd3dCommandQueue,
            APP_NUM_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN,
            g_pd3dSrvDescHeap, UiSrvDescriptorAlloc, UiSrvDescriptorFree, nullptr});
    uiConfig.effects = std::move(effects);
    uiConfig.contentScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    uiConfig.configureFonts = [haveUnifont, unifontPath](ImGuiIO& io, ImFont*& regular, ImFont*& title)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 10.0f; style.ChildRounding = 8.0f;
        style.FrameRounding = 6.0f; style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 6.0f; style.GrabRounding = 6.0f;
        style.WindowBorderSize = 0.0f; style.ChildBorderSize = 0.0f;
        style.WindowPadding = {};
        style.Colors[ImGuiCol_WindowBg] = style.Colors[ImGuiCol_ChildBg] =
            style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
        auto mergeUnifont = [&]() {
            if (!haveUnifont) return;
            ImFontConfig config; config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(unifontPath.c_str(), 16.0f, &config);
        };
        ImFontConfig base; base.OversampleH = 3; base.PixelSnapH = true;
        regular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &base);
        if (!regular) regular = io.Fonts->AddFontDefault();
        mergeUnifont();
        title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f, &base);
        if (!title) title = regular;
        mergeUnifont();
    };
    std::string uiError;
    g_ui = IVUI::UI::Create(std::move(uiConfig), &uiError);
    if (!g_ui)
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
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
        if (togglePressed && !g_ui->WantCaptureHotkey() && !g_ui->WantTextInput())
            show_menu = !show_menu;

        if ((g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) || ::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        g_ui->BeginFrame({ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd)});

        // The menu owns its own show/hide fade + scrim, so render it every frame.
        Demo::RenderMenu(*g_ui, &show_menu);

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
        IVUI::DX12::RenderFrame renderFrame;
        renderFrame.effects = {g_pd3dCommandList, g_mainRenderTargetResource[backBufferIdx],
            g_mainRenderTargetDescriptor[backBufferIdx], D3D12_RESOURCE_STATE_RENDER_TARGET,
            {(UINT)renderTargetSize.x, (UINT)renderTargetSize.y}, (float)ImGui::GetTime()};
        ID3D12DescriptorHeap* heaps[] = {g_pd3dSrvDescHeap};
        renderFrame.descriptorHeaps = heaps;
        renderFrame.descriptorHeapCount = 1;
        const IVUI::Theme& theme = g_ui->theme().Current();
        const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(theme.Accent);
        renderFrame.effects.renderBackground = true;
        renderFrame.effects.background.accent = {accent.x, accent.y, accent.z, accent.w};
        renderFrame.effects.background.secondary = {0.08f, 0.58f, 0.72f, 1.0f};
        renderFrame.effects.background.gridSize = 54.0f * theme.Scale;
        renderFrame.effects.background.gridStrength = 0.16f;
        renderFrame.effects.background.vignette = 0.62f;
        renderFrame.effects.background.animationSpeed = 1.0f;
        renderFrame.effects.renderBloom = theme.PanelGlow > 0.001f;
        renderFrame.effects.bloom.bounds = {
            {0.0f, 0.0f}, {renderTargetSize.x, renderTargetSize.y}};
        renderFrame.effects.bloom.sigma = theme.BloomSigma;
        renderFrame.effects.bloom.strength = std::clamp(theme.PanelGlow * 1.35f, 0.0f, 1.5f);
        renderFrame.effects.bloom.brightnessThreshold = theme.BloomThreshold;
        renderFrame.effects.bloom.saturationThreshold = theme.BloomSaturation;
        g_ui->Render(renderFrame);
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
    g_ui.reset();
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
    if (g_ui)
    {
        const IVUI::Win32::Event event{hWnd, msg, wParam, lParam};
        if (g_ui->ProcessEvent(IVUI::Win32::MakeEvent(event)))
            return true;
    }

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
