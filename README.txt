ImGUI Vector UI

ImGUI Vector UI is a C++20 Dear ImGui component library and DirectX 12 demo host.
It provides reusable vector-drawn controls, layout helpers, motion utilities,
theme tokens, GPU glass effects, and an injection-style demonstration menu.

Features

- Reusable widgets: buttons, toggles, sliders, combos, keybinds, popups, text
  input, color pickers, and toast notifications.
- Layout helpers: panels, sections, tabs, tables, scrolling regions, and status
  HUD lists.
- Parameterized theme system with Material You, VAPE, Neverlose, ImGui Classic,
  and iOS Liquid Glass presets.
- Theme-driven feature HUD layouts, including side rails, RGB frame effects, and
  translucent liquid-glass stacks.
- DirectX 12 host with a transparent DirectComposition window and custom GPU
  glass/glow pass.

Requirements

- Windows 10 or later
- Visual Studio 2022 C++ build tools
- Windows SDK 10.0.26100.0
- Git with submodule support

Setup

Clone the repository with submodules:

  git clone --recurse-submodules <repository-url>

For an existing clone:

  git submodule update --init --recursive

Build

Run one of the following commands from the repository root:

  build.bat Debug
  build.bat Release

The demo executable is generated under out\bin\x64\<configuration>.
The out directory and Visual Studio workspace data are intentionally ignored by
Git.

Dependencies

Dear ImGui is included as the third_party\imgui Git submodule and remains under
its own upstream license. The bundled Unifont fallback remains under its own
license. Project source code is available under the MIT License in LICENSE.
