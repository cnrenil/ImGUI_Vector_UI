# ImGui Vector UI 1.0

ImGui Vector UI is a C++20 object-oriented component library for Dear ImGui. A single
`IVUI::UI` instance owns the Dear ImGui context, UI state, theme, animation registry,
platform adapter, renderer adapter and optional GPU effect renderer.

The application retains ownership of its native window, graphics device, swapchain,
command queue and presentation loop. Native objects passed to IVUI are borrowed and
must outlive the `UI` instance.

## Modules

- `ui.widgets()`: buttons, toggles, checkbox/radio controls, sliders, combos, inputs,
  animated text, color and keybind editors, progress, popup and notifications.
- `ui.layouts()`: panels, sections, tabs, tables, rows, columns, grids, responsive
  columns, scrolling, status overlays and virtualized lists.
- `ui.theme()`: presets, mutable theme tokens, animated target themes, fonts and DPI.
- `ui.motion()`: ID-scoped value, toggle and keyed transitions.
- `ui.elements()`: theme-resolved effect and motion scopes for arbitrary ImGui geometry.
- `ui.effects()`: glass, blur, shadow, glow, bloom, exact item effects and custom
  effect registration.

Phase 4 modules are built by default alongside the core target while keeping their
third-party implementation sources isolated:

- `ImGuiVectorUI::Charts`: themed line, bar, area, scatter and real-time ImPlot views
  with legends, zooming, hover markers, tooltips and entry motion.
- `ImGuiVectorUI::Gltf`: a cgltf/stb_image `.gltf`/`.glb` loader for meshes, materials,
  UVs and decoded RGBA images.
- `ImGuiVectorUI::ModelViewer`: a backend-neutral viewer with orbit, pan, zoom, mesh
  selection, material inspection, lighting, grid, backgrounds and CPU offscreen frames.
- `ImGuiVectorUIModelViewerDX11`, `ImGuiVectorUIModelViewerDX12` and
  `ImGuiVectorUIModelViewerVulkan`: host-synchronized texture bridges that upload
  offscreen RGBA frames to native ImGui textures without leaking those types into core.

Self-drawn interactive controls participate in Dear ImGui keyboard/gamepad navigation.
All mutable library state belongs to the owning `UI`; multiple instances may be used
serially on their creation thread.

Exact item effects capture the geometry emitted while an `ItemEffectScope` is alive.
The captured source preserves each draw command's texture and clip rectangle, so glow,
shadow and blur follow glyph, icon and arbitrary mesh alpha instead of a bounding box:

```cpp
IVUI::ItemEffectRequest effects;
effects.Add(IVUI::ItemEffectLayer::Shadow(
    {0.0f, 0.0f, 0.0f, 0.75f}, {0.0f, 5.0f}, 7.0f, 0.45f));
effects.Add(IVUI::ItemEffectLayer::Glow(
    {0.2f, 0.8f, 1.0f, 1.0f}, 8.0f, 0.6f));

{
    auto scope = ui->effects().BeginItem(effects);
    ImGui::TextUnformatted("Exact text glow");
}
```

An item scope must end in the same ImGui window and draw-list channel where it began.
Scopes are intentionally non-nestable; combine up to four layers in one request.
DX12 and Vulkan frame slots may only be reused after the host has waited for the
corresponding GPU fence. Vulkan item effects are opt-in at renderer creation and
require `allowTargetCapture` on every frame that uses them; source-replacing blur
currently remains a DirectX capability.

Themes expose per-role defaults through `Theme::ForElement()`. Each role owns an
effect stack and an `ItemMotionStyle`. `ui.elements()` applies both in the required
scope order to any ImGui item, while per-item options can replace either value:

```cpp
IVUI::ElementOptions options;
options.role = IVUI::ElementRole::Text;
auto element = ui->elements().Begin("status_text", showStatus, options);
if (element)
    ImGui::TextUnformatted("Connected");
```

Item motion changes generated vertex alpha, position, and center scale after drawing.
Call the scope every frame while entering or exiting; its boolean conversion reports
whether the body still needs to be submitted. The ImGui layout and interaction hit box
remain at their final position.

When using the low-level APIs, create the effect scope first and the motion scope
second. C++ reverse destruction then transforms vertices before effect capture. The
combined `ElementScope` enforces this ordering. All scopes must end before `UI::Render()`.

Buttons use one options-based API; there is no extended compatibility entry point:

```cpp
IVUI::ButtonOptions button;
button.prominent = true;
button.element.effects = &customEffects; // null inherits the theme role
ui->widgets().Button("Apply", button);
```

Notifications use one queue and renderer. Dynamic Island is a per-notification style,
not a separate notification path. The theme supplies style, placement, viewport
padding, width, spacing, lifetime, interaction policy, role effects and role motion.
Every optional field can be overridden independently; `Custom` positions are absolute
screen coordinates and queued effect/motion values are copied:

```cpp
IVUI::NotificationOptions notification;
notification.type = IVUI::NotificationType::Success;
notification.style = IVUI::NotificationStyle::DynamicIsland;
notification.placement = IVUI::NotificationPlacement::TopCenter;
notification.duration = 5.0f;
const auto handle = ui->widgets().Notify("Saved", "Configuration updated", notification);
ui->widgets().SetNotificationProgress(handle, 0.75f);
IVUI::NotificationUpdate update;
update.title = "Saved again";
update.body = "Updated body";
ui->widgets().UpdateNotification(handle, update);
ui->widgets().CloseNotification(handle);
```

`AnimatedText` decodes UTF-8 codepoints individually, automatically wraps to the
available content width, and reserves the complete final layout. Rainbow,
`GradientFlow`, `Wave`, `Flicker`, `ColorPulse`, `FadeIn` and `Typewriter` flags can
be combined:

```cpp
IVUI::AnimatedTextOptions text;
text.animation = IVUI::TextAnimation::Rainbow |
                 IVUI::TextAnimation::Wave |
                 IVUI::TextAnimation::FadeIn;
ui->widgets().AnimatedText("Live status##status", text);
```

After initializing the repository submodules, the charts and model modules are built
by the standard configure command. No extra module switches are required:

```powershell
.\build-cmake.ps1 -Vulkan
```

ImPlot, cgltf, stb and GLM are tracked under `third_party/implot`, `third_party/cgltf`,
`third_party/stb` and `third_party/glm`. Override the corresponding `IVUI_*_SOURCE_DIR`
cache entries only when using alternate checkouts. These dependencies remain private
to their module targets and do not appear in portable core headers.

## Repository Layout

```text
components/ImGuiVectorUI/  Standalone component, public API, backends and tests
apps/Dx11Host/             Win32/DX11 example host
apps/Dx12Host/             Win32/DX12 example host
apps/DemoMenu/             Shared demo menu, font and runtime assets
apps/VulkanHost/           Win32/Vulkan example host
tests/package-consumer/    Minimal installed-package consumer
third_party/imgui/         Workspace-provided Dear ImGui dependency
third_party/implot/        Workspace-provided ImPlot dependency
third_party/cgltf/         Workspace-provided cgltf dependency
third_party/stb/           Workspace-provided stb_image dependency
third_party/glm/           Workspace-provided GLM dependency
```

The root CMake project only selects workspace options and composes these parts. The
component has its own `CMakeLists.txt`, package configuration, shaders, tests, license
and integration guide, so it can move into another repository without carrying the
demo or workspace build policy.

## Theme And Layout API

The component separates theme construction from theme consumption. Use
`IVUI::ThemeDefinition` with `ThemePalette`, `ThemeMetrics`, `ThemeShape` and an
`ItemMotionStyle` to add a new visual language, then pass the generated `Theme` to
`ui->theme().SetTarget()`. Built-in `ThemePalette::Dark()`,
`ThemePalette::Light()`, `ThemeMetrics::Comfortable()` and
`ThemeMetrics::Compact()` cover common application variants. Semantic status colors
are available as `Theme::Success`, `Warning`, `Error` and `Info`.

For page composition, declare `IVUI::ContentBlock` values and call
`ui->layouts().Content()`. The same content can be rendered as responsive tiles,
multi-column single-window content, or separate ImGui windows by changing
`ContentLayoutOptions::mode`. `ui->layouts().Workbench()` adds a responsive shell
with a title bar, navigation region, body and status band. Host code only supplies
the block IDs and drawing callbacks; IVUI handles container geometry, scrolling,
compact reflow and window lifecycle.

## Build

For this repository's relocated, registry-less Visual Studio installation on `A:`,
use the included script. It configures the compiler, Windows SDK, CMake, NMake and
Debug CRT paths explicitly:

```powershell
.\build-cmake.ps1 -Clean
.\build-cmake.ps1 -Configuration Debug
.\build-cmake.ps1 -Configuration Release -InstallPrefix out\stage
.\build-cmake.ps1 -Configuration Release -InstallPrefix out\stage -VerifyPackage
.\build-cmake.ps1 -ComponentOnly -NoDemo -BuildDirectory out\component
```

Useful switches are `-NoDemo`, `-NoTests`, `-Vulkan`, `-ComponentOnly` and
`-BuildDirectory`. Tool locations can be overridden with `-VisualStudioRoot`, `-MsvcVersion`,
`-WindowsSdkRoot` and `-WindowsSdkVersion`. Extra cache entries can be supplied with
`-CMakeArguments '-DNAME=VALUE'`. `-VerifyPackage` builds the independent consumer
against the installed package and requires `-InstallPrefix`.

The default Windows demo build creates the DX11 and DX12 hosts. To build all three
hosts, set `VULKAN_SDK` and enable Vulkan:

```powershell
$env:VULKAN_SDK = 'D:\Develop_Environments\VulkanSDK\1.4.357.0'
.\build-cmake.ps1 -Clean -Vulkan
```

The executables are named `ImGuiVectorUIDemoDX11`, `ImGuiVectorUIDemo` (DX12), and
`ImGuiVectorUIDemoVulkan`. Compiled effect shaders are embedded in the component
library, so the executables do not require a runtime shader directory. The demo font
and its license remain separate runtime assets.

On a normally registered development environment, standard CMake commands remain
available:

```powershell
cmake -S . -B build `
  -DIVUI_BUILD_DEMO=ON `
  -DIVUI_BUILD_DX11_DEMO=ON `
  -DIVUI_BUILD_DX11_EFFECTS=ON `
  -DIVUI_BUILD_DX12_EFFECTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The component can also be configured independently:

```powershell
cmake -S components/ImGuiVectorUI -B out/component `
  -DIVUI_IMGUI_SOURCE_DIR="$PWD/third_party/imgui" `
  -DIVUI_BUILD_TESTS=ON
cmake --build out/component --config Release
ctest --test-dir out/component -C Release --output-on-failure
```

## Use From Another Project

When vendoring the component, provide an existing `imgui` or `imgui::imgui` target,
or set `IVUI_IMGUI_SOURCE_DIR` before adding the component:

```cmake
set(IVUI_IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(IVUI_BUILD_DX12_EFFECTS ON)
set(IVUI_BUILD_WIN32_PLATFORM ON)
add_subdirectory(vendor/ImGuiVectorUI)

target_link_libraries(my_app PRIVATE
    ImGuiVectorUI::ImGuiVectorUI
    ImGuiVectorUI::PlatformWin32
    ImGuiVectorUI::RendererDX12)
```

Embedded builds default all native backends and install rules to `OFF`. Installed
packages use the same target names:

```cmake
find_package(ImGuiVectorUI 1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ImGuiVectorUI::ImGuiVectorUI)
```

Vulkan is optional and requires a Vulkan SDK containing `glslangValidator`:

```powershell
cmake -S . -B build-vulkan -DIVUI_BUILD_DEMO=OFF -DIVUI_BUILD_VULKAN_EFFECTS=ON
cmake --build build-vulkan --config Release
```

`ImGuiVectorUIDirectXShaders` and `ImGuiVectorUIVulkanShaders` compile the
component's HLSL/GLSL sources. DX11 and DX12 share the same DirectX shader target
and bytecode. Generated binaries are embedded into `ImGuiVectorUI::ImGuiVectorUI`
and accessed exclusively through `IVUI::ShaderLibrary`; runtime shader replacement
and external shader directories are not supported.

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

std::string error;
auto effects = IVUI::DX12::EffectRenderer::Create(
    {device, renderTargetFormat}, &error);

IVUI::UIConfig config;
config.platform = std::make_unique<IVUI::Win32::PlatformBackend>(hwnd);
config.renderer = std::make_unique<IVUI::DX12::RendererBackend>(rendererCreateInfo);
config.effects = std::move(effects);
config.contentScale = initialDpiScale;
config.configureFonts = [](ImGuiIO& io, ImFont*& regular, ImFont*& title) {
    regular = io.Fonts->AddFontDefault();
    title = regular;
};

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
- Dear ImGui 1.92.9 from the workspace's `third_party/imgui`, another source tree
  selected with `IVUI_IMGUI_SOURCE_DIR`, or an existing `imgui`/`imgui::imgui` target
- Windows SDK for Win32, Direct3D 11 and Direct3D 12 targets
- Vulkan SDK only when `IVUI_BUILD_VULKAN_EFFECTS=ON`

Project source is MIT licensed. Dear ImGui and the bundled Unifont retain their own
licenses.
