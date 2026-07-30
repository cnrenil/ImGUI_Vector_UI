# ImGuiVectorUI Component

This directory is the independently buildable ImGuiVectorUI C++20 component. It
contains the public headers, implementation, optional renderer/effect backends,
DirectX and Vulkan shaders, tests, package configuration and license.

DX11 and DX12 compile the same HLSL stage sources under `shaders/directx`, with one
`.frag` file per effect. Vulkan follows the same layout under `shaders/vulkan`.
All enabled backend binaries are embedded into the component library and managed by
`IVUI::ShaderLibrary`. Runtime shader replacement is intentionally unsupported.

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

Optional targets are controlled by `IVUI_BUILD_WIN32_PLATFORM`,
`IVUI_BUILD_DX11_EFFECTS`, `IVUI_BUILD_DX12_EFFECTS` and
`IVUI_BUILD_VULKAN_EFFECTS`. These options default to `OFF` when the component
is embedded, so a parent project opts into only the native backends it uses.

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
`ImGuiVectorUI::ImGuiVectorUI` plus any enabled backend targets. Shader binaries are
already contained in the installed component library and require no runtime assets.
