# ImGuiVectorUI Component

This directory is the independently buildable ImGuiVectorUI C++20 component. It
contains the public headers, implementation, optional renderer/effect backends,
DirectX and Vulkan shaders, tests, package configuration and license.

DX11 and DX12 compile the same HLSL stage sources under `shaders/directx`, with one
`.frag` file per effect. Vulkan follows the same layout under `shaders/vulkan`.
All enabled backend binaries are embedded into the component library and managed by
`IVUI::ShaderLibrary`. Runtime shader replacement is intentionally unsupported.

The portable item-effect protocol captures ImGui geometry in the core and replays it
through whichever optional renderer is enabled. Platform and graphics API types remain
confined to backend headers and targets.

Item motion is implemented entirely in the portable core by transforming vertices
emitted inside a scope. It adds no graphics dependency or backend-specific state.

`ElementScope` combines a theme role's effect stack and motion for arbitrary ImGui
geometry. Passing a pointer to an empty `ItemEffectRequest` disables inherited effects;
null effect and motion pointers inherit the selected role.

The widget API intentionally has no legacy overload layer. `Button` accepts
`ButtonOptions`; `Notify` accepts independently optional style, placement, custom
position, sizing, lifetime, interaction, progress, action, effect and motion values.
`NotificationStyle::DynamicIsland` is handled by the same queue, layout and lifecycle
as card notifications. `Theme::Notifications` and `ElementRole::Notification` define
the defaults.

Animated UTF-8 text supports composable gradient-flow, rainbow, wave, flicker,
color-pulse, fade-in and typewriter flags. It automatically wraps to the available
width, reserves the complete final bounds and inherits `ElementRole::Text` effects
and motion unless the item overrides them.

## Phase 4 Modules

The component builds three module targets by default without adding their dependencies
to the core library:

- `ImGuiVectorUI::Charts` wraps ImPlot with line, bar, area, scatter and real-time
  stream APIs; themes, hover tooltips, legends, zooming and entry motion are owned by
  IVUI, and no ImPlot types appear in the public API.
- `ImGuiVectorUI::Gltf` uses cgltf and stb_image to load meshes, normals, UVs, materials,
  textures and decoded RGBA images from `.gltf` and `.glb` files.
- `ImGuiVectorUI::ModelViewer` renders those structures through ImDrawList or a CPU
  offscreen RGBA frame. It supports orbit, pan, wheel zoom, mesh selection, material
  inspection, environment light, grid, selectable backgrounds and wireframe/point modes.
- `ImGuiVectorUIModelViewerDX11`, `ImGuiVectorUIModelViewerDX12` and
  `ImGuiVectorUIModelViewerVulkan` are host-synchronized native texture bridges. They
  are separate from the portable target and require the matching backend option.

The default paths resolve to the `third_party/implot`, `third_party/cgltf`,
`third_party/stb` and `third_party/glm` submodules. Set `IVUI_IMPLOT_SOURCE_DIR`,
`IVUI_CGLTF_SOURCE_DIR`, `IVUI_STB_SOURCE_DIR` or `IVUI_GLM_SOURCE_DIR` for alternate
checkouts. The three module targets remain separate from
`ImGuiVectorUI::ImGuiVectorUI`, but are always available from the component build and
package.

## Design Tokens And Layouts

Themes are built from semantic design tokens instead of widget-specific colors:

```cpp
IVUI::ThemeDefinition definition;
definition.palette = IVUI::ThemePalette::Light();
definition.metrics = IVUI::ThemeMetrics::Compact();
definition.shape.controlRounding = 4.0f;
definition.motion = IVUI::ItemMotionStyle::Fade(0.12f);
IVUI::Theme theme = IVUI::MakeTheme(definition);
ui->theme().SetTarget(theme);
```

`ThemePalette` owns surfaces, text, borders, accent, success, warning, error and
info colors. `ThemeMetrics` owns density and typography. `ThemeShape` owns geometry.
The generated `Theme` remains mutable for advanced effects and per-role overrides.
This keeps new themes small while preserving compatibility with existing presets.

The layout library accepts content blocks and supplies three compositions:

```cpp
const IVUI::ContentBlock blocks[] = {
    {"settings", "Settings", [](IVUI::UI& ui) {
        ui.widgets().Toggle("Enabled", &enabled);
    }},
    {"preview", "Preview", [](IVUI::UI& ui) {
        ui.widgets().ProgressBar("Progress", progress);
    }}
};

IVUI::ContentLayoutOptions layout;
layout.mode = IVUI::ContentLayout::Columns;
layout.minimumColumnWidth = 280.0f;
ui->layouts().Content("settings-page", blocks, layout);
```

`ContentLayout::Tiled` creates equal-height responsive tiles, `Columns` creates
scrollable vertical columns, and `Windows` creates independently closable ImGui
windows. Block IDs are stable and must be unique. The library owns the layout
containers; callbacks only describe the content.

`ui->layouts().Workbench()` provides a reusable header, responsive navigation,
content region and status band. Navigation receives a compact-mode flag so the
same page can switch from tabs/sidebar to a selector on narrow windows.

## Embed In Another CMake Project

Provide Dear ImGui as an `imgui` or `imgui::imgui` target through the parent
project's package manager before adding the component:

```cmake
find_package(imgui CONFIG REQUIRED)
add_subdirectory(vendor/ImGuiVectorUI)

target_link_libraries(my_app PRIVATE ImGuiVectorUI::ImGuiVectorUI)
```

If the ImGui project does not define a target, point the component at its source
tree instead:

```cmake
set(IVUI_IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
add_subdirectory(components/ImGuiVectorUI)
```

Optional targets are limited to `IVUI_BUILD_WIN32_PLATFORM`,
`IVUI_BUILD_DX11_EFFECTS`, `IVUI_BUILD_DX12_EFFECTS` and
`IVUI_BUILD_VULKAN_EFFECTS`. The Charts, Gltf and ModelViewer targets are always
built; only their source directories can be overridden for alternate checkouts.

## Standalone Build

From this repository, the relocated-toolchain helper has a component-only mode:

```powershell
.\build-cmake.ps1 -ComponentOnly -NoDemo -BuildDirectory out\component
```

With a normally registered toolchain, configure the component directly:

```powershell
cmake -S components/ImGuiVectorUI -B out/component `
  -DIVUI_IMGUI_SOURCE_DIR="$PWD/third_party/imgui" `
  -DIVUI_BUILD_TESTS=ON
cmake --build out/component --config Release
ctest --test-dir out/component -C Release --output-on-failure
```

Set `IVUI_INSTALL=ON` to generate an installable CMake package. Installed
projects consume it with `find_package(ImGuiVectorUI CONFIG REQUIRED)` and link
`ImGuiVectorUI::ImGuiVectorUI`, `ImGuiVectorUI::Charts`, `ImGuiVectorUI::Gltf`,
`ImGuiVectorUI::ModelViewer` plus any enabled backend targets, including the matching
`ImGuiVectorUI::ModelViewerDX11`, `ImGuiVectorUI::ModelViewerDX12` or
`ImGuiVectorUI::ModelViewerVulkan` bridge. Shader binaries are already contained in the
installed component library and require no runtime assets.
