# ImGui Vector UI 1.0

ImGui Vector UI is a C++20 object-oriented component library for Dear ImGui. A single
`IVUI::UI` instance owns the Dear ImGui context, UI state, theme, animation registry,
platform adapter, renderer adapter and optional GPU effect renderer.

The application retains ownership of its native window, graphics device, swapchain,
command queue and presentation loop. Native objects passed to IVUI are borrowed and
must outlive the `UI` instance.

## Modules

- `ui.widgets()`: buttons, toggles, checkbox/radio controls, sliders, combos, inputs,
  color and keybind editors, selection controls, progress, spinner, popup and toast.
- `ui.layouts()`: panels, sections, tabs, tables, rows, columns, grids, responsive
  columns, scrolling, status overlays and virtualized lists.
- `ui.theme()`: presets, mutable theme tokens, animated target themes, fonts and DPI.
- `ui.motion()`: ID-scoped value, toggle and keyed transitions.
- `ui.effects()`: glass, blur, shadow, glow, bloom and custom effect registration.

Self-drawn interactive controls participate in Dear ImGui keyboard/gamepad navigation.
All mutable library state belongs to the owning `UI`; multiple instances may be used
serially on their creation thread.

## Build

For this repository's relocated, registry-less Visual Studio installation on `A:`,
use the included script. It configures the compiler, Windows SDK, CMake, NMake and
Debug CRT paths explicitly:

```powershell
.\build-cmake.ps1 -Clean
.\build-cmake.ps1 -Configuration Debug
.\build-cmake.ps1 -Configuration Release -InstallPrefix out\stage
```

Useful switches are `-NoDemo`, `-NoTests`, `-Vulkan` and `-BuildDirectory`. Tool
locations can be overridden with `-VisualStudioRoot`, `-MsvcVersion`,
`-WindowsSdkRoot` and `-WindowsSdkVersion`. Extra cache entries can be supplied with
`-CMakeArguments '-DNAME=VALUE'`.

On a normally registered development environment, standard CMake commands remain
available:

```powershell
cmake -S . -B build \
  -DIVUI_BUILD_DEMO=ON \
  -DIVUI_BUILD_DX11_EFFECTS=ON \
  -DIVUI_BUILD_DX12_EFFECTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Vulkan is optional and requires a Vulkan SDK containing `glslangValidator`:

```powershell
cmake -S . -B build-vulkan -DIVUI_BUILD_DEMO=OFF -DIVUI_BUILD_VULKAN_EFFECTS=ON
cmake --build build-vulkan --config Release
```

The `ImGuiVectorUIVulkanShaders` target compiles the GLSL files under `shaders/` to
SPIR-V. `IVUI::Vulkan::EffectShaderLibrary::Load()` loads the generated set.

## CMake Targets

```cmake
ImGuiVectorUI::ImGuiVectorUI
ImGuiVectorUI::PlatformWin32
ImGuiVectorUI::RendererDX11
ImGuiVectorUI::EffectsDX11
ImGuiVectorUI::RendererDX12
ImGuiVectorUI::EffectsDX12
ImGuiVectorUI::RendererVulkan
ImGuiVectorUI::EffectsVulkan
```

Only enabled targets are exported. The portable core does not include Windows or
Vulkan headers unless a backend-specific header is included.

## DX12 Setup

```cpp
#include <ImGuiVectorUI/ImGuiVectorUI.h>
#include <ImGuiVectorUI/Backends/PlatformWin32.h>
#include <ImGuiVectorUI/Backends/RendererDX12.h>

auto effects = IVUI::DX12::EffectRenderer::Create({device, renderTargetFormat});

IVUI::UIConfig config;
config.platform = std::make_unique<IVUI::Win32::PlatformBackend>(hwnd);
config.renderer = std::make_unique<IVUI::DX12::RendererBackend>(rendererCreateInfo);
config.effects = std::move(effects);
config.contentScale = initialDpiScale;
config.configureFonts = [](ImGuiIO& io, ImFont*& regular, ImFont*& title) {
    regular = io.Fonts->AddFontDefault();
    title = regular;
};

std::string error;
auto ui = IVUI::UI::Create(std::move(config), &error);
```

Forward platform events and render one frame:

```cpp
const IVUI::Win32::Event event{hwnd, message, wParam, lParam};
ui->ProcessEvent(IVUI::Win32::MakeEvent(event));

ui->BeginFrame({dpiScale});
ui->layouts().Panel("Settings", {100, 100}, {420, 640}, [&] {
    ui->widgets().Toggle("Enabled", &enabled);
    ui->widgets().SliderFloat("Opacity", &opacity, 0.0f, 1.0f);
});

IVUI::DX12::RenderFrame frame;
frame.effects = {commandList, target, targetRtv,
                 D3D12_RESOURCE_STATE_RENDER_TARGET, {width, height}, timeSeconds};
frame.effects.renderBackground = true;
frame.effects.background.accent = {1.0f, 0.35f, 0.3f, 1.0f};
frame.effects.renderBloom = true;
frame.effects.bloom.bounds = {{0.0f, 0.0f}, {float(width), float(height)}};
frame.effects.bloom.sigma = 3.0f;
frame.effects.bloom.strength = 0.3f;
ui->Render(frame, &error);
```

The host records/submits command lists and calls `Present`; `UI::Render()` renders
Dear ImGui and executes effects at their submitted draw-list depth.

## Custom Effects

IDs at or above `EffectId::CustomFirst` can be registered per `UI` instance. The
request contains 64 bytes of inline custom data, copied safely into the ImGui draw
command. A custom callback receives the concrete backend renderer after its frame has
been activated.

```cpp
const auto id = static_cast<IVUI::EffectId>(
    static_cast<unsigned>(IVUI::EffectId::CustomFirst) + 1);
ui->effects().Register(id, RenderCustomEffect, userData);

IVUI::EffectRequest request;
request.id = id;
ui->effects().Submit(request);
```

## Dependencies

- C++20 and CMake 3.21+
- Dear ImGui 1.92.9 from `third_party/imgui`, or an existing `imgui`/`imgui::imgui`
  CMake target
- Windows SDK for Win32, Direct3D 11 and Direct3D 12 targets
- Vulkan SDK only when `IVUI_BUILD_VULKAN_EFFECTS=ON`

Project source is MIT licensed. Dear ImGui and the bundled Unifont retain their own
licenses.
