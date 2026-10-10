# ImGuiVectorUI Agent Notes

## Build Environment

- This Windows workspace uses an unregistered Visual Studio/MSVC installation on `A:`.
- Do not search system drives for CMake, MSVC, the Windows SDK, or runtime files.
- Read and use `build-cmake.ps1`; it configures all compiler, SDK, NMake, include,
  library, CRT, and CMake paths explicitly.
- Component validation:
  `./build-cmake.ps1 -ComponentOnly -NoDemo -Vulkan -BuildDirectory out/component`
- Full validation including all demo hosts:
  `./build-cmake.ps1 -Vulkan -BuildDirectory out/full`
- Package validation requires both `-InstallPrefix` and `-VerifyPackage`.
- Vulkan currently uses `VULKAN_SDK=D:\Develop_Environments\VulkanSDK\1.4.357.0`.

## Project Principles

- `components/ImGuiVectorUI` must remain independently buildable and installable.
- Native window, graphics device, swapchain, queues, command lists, and presentation
  remain host-owned and are borrowed by IVUI.
- Core/public portable headers must not leak DirectX, Vulkan, or Win32 types.
- DX11, DX12, and Vulkan are optional targets and should retain behavioral parity.
- Effect shaders are compiled at build time, embedded through `ShaderLibrary`, and are
  not compiled or replaced at runtime.
- Dear ImGui is the only mandatory dependency; new large features should be optional
  modules when they introduce plotting, model-loading, or graphics dependencies.
- Mutable state belongs to the owning `UI`/IVUI context. Multiple UI instances are
  serial, owner-thread objects.
- Frame-resource indices are host synchronization contracts: a slot may only be reused
  after its GPU fence has completed.
- Preserve existing user changes and avoid unrelated refactors.
