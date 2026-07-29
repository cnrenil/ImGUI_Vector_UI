cmake_minimum_required(VERSION 3.21)

project(ImGuiVectorUIWorkspace VERSION 1.0.0 LANGUAGES CXX)

option(IVUI_BUILD_DEMO "Build the Win32/DX12 demonstration host" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_BUILD_TESTS "Build component tests" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_BUILD_DX12_EFFECTS "Build the optional DirectX 12 effects backend" ${WIN32})
option(IVUI_BUILD_DX11_EFFECTS "Build the optional DirectX 11 effects backend" ${WIN32})
option(IVUI_BUILD_VULKAN_EFFECTS "Build the optional Vulkan effects backend" OFF)
option(IVUI_BUILD_WIN32_PLATFORM "Build the Win32 platform adapter" ${WIN32})
option(IVUI_INSTALL "Generate install and package export rules" ${PROJECT_IS_TOP_LEVEL})

set(IVUI_IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui" CACHE PATH
    "Dear ImGui source tree used when no imgui target is supplied")

if(IVUI_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(components/ImGuiVectorUI)

if(IVUI_BUILD_DEMO)
    add_subdirectory(apps/Dx12Host)
endif()
