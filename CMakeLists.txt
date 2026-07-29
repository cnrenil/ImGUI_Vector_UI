cmake_minimum_required(VERSION 3.21)

project(ImGuiVectorUI VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(IVUI_BUILD_DEMO "Build the Win32/DX12 demonstration host" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_BUILD_DX12_EFFECTS "Build the optional DirectX 12 effects backend" ${WIN32})
option(IVUI_BUILD_DX11_EFFECTS "Build the optional DirectX 11 effects backend" ${WIN32})
option(IVUI_BUILD_VULKAN_EFFECTS "Build the optional Vulkan effects backend" OFF)
option(IVUI_BUILD_WIN32_PLATFORM "Build the Win32 platform adapter" ${WIN32})
option(IVUI_BUILD_TESTS "Build core tests" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_USE_BUNDLED_IMGUI "Use third_party/imgui when no ImGui target is supplied" ON)

set(IVUI_BUNDLED_IMGUI OFF)
if(TARGET imgui::imgui AND NOT TARGET imgui)
    add_library(imgui ALIAS imgui::imgui)
endif()

if(NOT TARGET imgui)
    if(NOT IVUI_USE_BUNDLED_IMGUI)
        message(FATAL_ERROR "ImGuiVectorUI requires an existing 'imgui' CMake target or IVUI_USE_BUNDLED_IMGUI=ON.")
    endif()

    add_library(imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp)
    target_include_directories(imgui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/imgui>)
    target_compile_features(imgui PUBLIC cxx_std_20)
    set(IVUI_BUNDLED_IMGUI ON)
endif()

set(IVUI_CORE_SOURCES
    src/UI.cpp
    src/Core/AnimState.cpp
    src/Core/Context.cpp
    src/Core/DragDrop.cpp
    src/Core/Easing.cpp
    src/Core/Id.cpp
    src/Core/Motion.cpp
    src/Drawing/DrawText.cpp
    src/Drawing/EffectsBridge.cpp
    src/Drawing/Icons.cpp
    src/Drawing/Primitives.cpp
    src/Drawing/Tooltip.cpp
    src/Layout/AutoSizeChild.cpp
    src/Layout/Flex.cpp
    src/Layout/Panel.cpp
    src/Layout/ScrollRegion.cpp
    src/Layout/Section.cpp
    src/Layout/StatusChips.cpp
    src/Layout/Table.cpp
    src/Layout/Tabs.cpp
    src/Theme/Theme.cpp
    src/Widgets/Button.cpp
    src/Widgets/Basic.cpp
    src/Widgets/ColorPicker.cpp
    src/Widgets/Combo.cpp
    src/Widgets/HotkeyInput.cpp
    src/Widgets/Keybind.cpp
    src/Widgets/Popup.cpp
    src/Widgets/Slider.cpp
    src/Widgets/TextInput.cpp
    src/Widgets/Toast.cpp
    src/Widgets/Toggle.cpp)

add_library(ImGuiVectorUI STATIC ${IVUI_CORE_SOURCES})
add_library(ImGuiVectorUI::ImGuiVectorUI ALIAS ImGuiVectorUI)
target_compile_features(ImGuiVectorUI PUBLIC cxx_std_20)
target_compile_definitions(ImGuiVectorUI PRIVATE IMGUI_DEFINE_MATH_OPERATORS IVUI_INTERNAL_API)
# ImGui is supplied by the parent project at build time and by Config.cmake after
# installation. Keep the target name as an install-time string so it is not
# incorrectly required to be part of this package's export set.
target_link_libraries(ImGuiVectorUI PUBLIC imgui)
target_include_directories(ImGuiVectorUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(IVUI_BUILD_DX12_EFFECTS)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_DX12_EFFECTS requires Windows.")
    endif()
    add_library(ImGuiVectorUIEffectsDX12 STATIC src/Drawing/EffectsDX12.cpp)
    add_library(ImGuiVectorUI::EffectsDX12 ALIAS ImGuiVectorUIEffectsDX12)
    set_target_properties(ImGuiVectorUIEffectsDX12 PROPERTIES EXPORT_NAME EffectsDX12)
    target_compile_features(ImGuiVectorUIEffectsDX12 PUBLIC cxx_std_20)
    target_link_libraries(ImGuiVectorUIEffectsDX12 PUBLIC ImGuiVectorUI)
    target_include_directories(ImGuiVectorUIEffectsDX12 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(ImGuiVectorUIEffectsDX12 PRIVATE d3d12 dxgi d3dcompiler)
endif()

if(IVUI_BUILD_WIN32_PLATFORM)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_WIN32_PLATFORM requires Windows.")
    endif()
    add_library(ImGuiVectorUIPlatformWin32 STATIC
        src/Backends/PlatformWin32.cpp
        third_party/imgui/backends/imgui_impl_win32.cpp)
    add_library(ImGuiVectorUI::PlatformWin32 ALIAS ImGuiVectorUIPlatformWin32)
    set_target_properties(ImGuiVectorUIPlatformWin32 PROPERTIES EXPORT_NAME PlatformWin32)
    target_compile_features(ImGuiVectorUIPlatformWin32 PUBLIC cxx_std_20)
    target_compile_definitions(ImGuiVectorUIPlatformWin32 PRIVATE UNICODE _UNICODE)
    target_include_directories(ImGuiVectorUIPlatformWin32
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE third_party/imgui/backends)
    target_link_libraries(ImGuiVectorUIPlatformWin32 PUBLIC ImGuiVectorUI PRIVATE user32)
endif()

if(IVUI_BUILD_DX11_EFFECTS)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_DX11_EFFECTS requires Windows.")
    endif()
    add_library(ImGuiVectorUIEffectsDX11 STATIC src/Drawing/EffectsDX11.cpp)
    add_library(ImGuiVectorUI::EffectsDX11 ALIAS ImGuiVectorUIEffectsDX11)
    set_target_properties(ImGuiVectorUIEffectsDX11 PROPERTIES EXPORT_NAME EffectsDX11)
    target_compile_features(ImGuiVectorUIEffectsDX11 PUBLIC cxx_std_20)
    target_link_libraries(ImGuiVectorUIEffectsDX11 PUBLIC ImGuiVectorUI PRIVATE d3d11 d3dcompiler)
    target_include_directories(ImGuiVectorUIEffectsDX11 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    add_library(ImGuiVectorUIRendererDX11 STATIC
        src/Backends/RendererDX11.cpp
        third_party/imgui/backends/imgui_impl_dx11.cpp)
    add_library(ImGuiVectorUI::RendererDX11 ALIAS ImGuiVectorUIRendererDX11)
    set_target_properties(ImGuiVectorUIRendererDX11 PROPERTIES EXPORT_NAME RendererDX11)
    target_compile_features(ImGuiVectorUIRendererDX11 PUBLIC cxx_std_20)
    target_include_directories(ImGuiVectorUIRendererDX11
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE third_party/imgui/backends)
    target_link_libraries(ImGuiVectorUIRendererDX11 PUBLIC ImGuiVectorUIEffectsDX11 PRIVATE d3d11)
endif()

if(IVUI_BUILD_DX12_EFFECTS)
    add_library(ImGuiVectorUIRendererDX12 STATIC
        src/Backends/RendererDX12.cpp
        third_party/imgui/backends/imgui_impl_dx12.cpp)
    add_library(ImGuiVectorUI::RendererDX12 ALIAS ImGuiVectorUIRendererDX12)
    set_target_properties(ImGuiVectorUIRendererDX12 PROPERTIES EXPORT_NAME RendererDX12)
    target_compile_features(ImGuiVectorUIRendererDX12 PUBLIC cxx_std_20)
    target_include_directories(ImGuiVectorUIRendererDX12
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE third_party/imgui/backends)
    target_link_libraries(ImGuiVectorUIRendererDX12 PUBLIC ImGuiVectorUIEffectsDX12 PRIVATE d3d12)
endif()

if(IVUI_BUILD_VULKAN_EFFECTS)
    find_package(Vulkan REQUIRED)
    add_library(ImGuiVectorUIEffectsVulkan STATIC src/Drawing/EffectsVulkan.cpp)
    add_library(ImGuiVectorUI::EffectsVulkan ALIAS ImGuiVectorUIEffectsVulkan)
    set_target_properties(ImGuiVectorUIEffectsVulkan PROPERTIES EXPORT_NAME EffectsVulkan)
    target_compile_features(ImGuiVectorUIEffectsVulkan PUBLIC cxx_std_20)
    target_link_libraries(ImGuiVectorUIEffectsVulkan PUBLIC ImGuiVectorUI Vulkan::Vulkan)
    target_include_directories(ImGuiVectorUIEffectsVulkan PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        find_program(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE glslangValidator REQUIRED)
    endif()
    set(IVUI_VULKAN_SHADER_OUTPUTS)
    foreach(shader effects.vert effects_blur.frag effects_material.frag effects_shape.frag effects_bloom.frag)
        set(output "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.spv")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
            COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V
                    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}"
            VERBATIM)
        list(APPEND IVUI_VULKAN_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(ImGuiVectorUIVulkanShaders DEPENDS ${IVUI_VULKAN_SHADER_OUTPUTS})
    add_dependencies(ImGuiVectorUIEffectsVulkan ImGuiVectorUIVulkanShaders)

    add_library(ImGuiVectorUIRendererVulkan STATIC
        src/Backends/RendererVulkan.cpp
        third_party/imgui/backends/imgui_impl_vulkan.cpp)
    add_library(ImGuiVectorUI::RendererVulkan ALIAS ImGuiVectorUIRendererVulkan)
    set_target_properties(ImGuiVectorUIRendererVulkan PROPERTIES EXPORT_NAME RendererVulkan)
    target_compile_features(ImGuiVectorUIRendererVulkan PUBLIC cxx_std_20)
    target_include_directories(ImGuiVectorUIRendererVulkan
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE third_party/imgui/backends)
    target_link_libraries(ImGuiVectorUIRendererVulkan PUBLIC ImGuiVectorUIEffectsVulkan Vulkan::Vulkan)
endif()

if(IVUI_BUILD_DEMO)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_DEMO requires Windows.")
    endif()
    if(NOT IVUI_BUILD_DX12_EFFECTS)
        message(FATAL_ERROR "IVUI_BUILD_DEMO requires IVUI_BUILD_DX12_EFFECTS=ON.")
    endif()
    add_executable(ImGuiVectorUIDemo WIN32
        app/Dx12Host/main.cpp
        src/DemoMenu.cpp)
    target_compile_features(ImGuiVectorUIDemo PRIVATE cxx_std_20)
    target_compile_definitions(ImGuiVectorUIDemo PRIVATE
        IMGUI_DEFINE_MATH_OPERATORS IVUI_INTERNAL_API UNICODE _UNICODE)
    target_include_directories(ImGuiVectorUIDemo PRIVATE src third_party/imgui/backends)
    target_link_libraries(ImGuiVectorUIDemo PRIVATE
        ImGuiVectorUIRendererDX12 ImGuiVectorUIPlatformWin32
        d3d12 dxgi dxguid user32 gdi32 shell32)
    add_custom_command(TARGET ImGuiVectorUIDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/unifont.otf
            $<TARGET_FILE_DIR:ImGuiVectorUIDemo>/unifont.otf)
endif()

if(IVUI_BUILD_TESTS)
    enable_testing()
    add_executable(ImGuiVectorUICoreTests tests/CoreTests.cpp)
    target_link_libraries(ImGuiVectorUICoreTests PRIVATE ImGuiVectorUI)
    add_test(NAME ImGuiVectorUICoreTests COMMAND ImGuiVectorUICoreTests)
    if(TARGET ImGuiVectorUIEffectsDX12)
        add_executable(ImGuiVectorUIEffectsDX12Tests tests/EffectsDX12Tests.cpp)
        target_link_libraries(ImGuiVectorUIEffectsDX12Tests PRIVATE ImGuiVectorUIEffectsDX12)
        add_test(NAME ImGuiVectorUIEffectsDX12Tests COMMAND ImGuiVectorUIEffectsDX12Tests)
    endif()
endif()

set(IVUI_INSTALL_TARGETS ImGuiVectorUI)
if(IVUI_BUNDLED_IMGUI)
    list(APPEND IVUI_INSTALL_TARGETS imgui)
endif()
if(TARGET ImGuiVectorUIEffectsDX12)
    list(APPEND IVUI_INSTALL_TARGETS ImGuiVectorUIEffectsDX12)
endif()
if(TARGET ImGuiVectorUIEffectsDX11)
    list(APPEND IVUI_INSTALL_TARGETS ImGuiVectorUIEffectsDX11)
endif()
if(TARGET ImGuiVectorUIEffectsVulkan)
    list(APPEND IVUI_INSTALL_TARGETS ImGuiVectorUIEffectsVulkan)
endif()
foreach(target ImGuiVectorUIPlatformWin32 ImGuiVectorUIRendererDX11
               ImGuiVectorUIRendererDX12 ImGuiVectorUIRendererVulkan)
    if(TARGET ${target})
        list(APPEND IVUI_INSTALL_TARGETS ${target})
    endif()
endforeach()
install(TARGETS ${IVUI_INSTALL_TARGETS} EXPORT ImGuiVectorUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/ImGuiVectorUI/ImGuiVectorUI.h include/ImGuiVectorUI/UI.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI)
install(FILES include/ImGuiVectorUI/Core/Types.h include/ImGuiVectorUI/Core/Easing.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Core)
install(FILES include/ImGuiVectorUI/Drawing/Effects.h include/ImGuiVectorUI/Drawing/Icons.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Drawing)
install(FILES include/ImGuiVectorUI/Layout/Flex.h include/ImGuiVectorUI/Layout/Panel.h
              include/ImGuiVectorUI/Layout/StatusChips.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Layout)
install(FILES include/ImGuiVectorUI/Theme/Theme.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Theme)
install(FILES include/ImGuiVectorUI/Widgets/Keybind.h include/ImGuiVectorUI/Widgets/Toast.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Widgets)
if(TARGET ImGuiVectorUIPlatformWin32)
    install(FILES include/ImGuiVectorUI/Backends/PlatformWin32.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Backends)
endif()
if(TARGET ImGuiVectorUIEffectsDX11)
    install(FILES include/ImGuiVectorUI/Backends/EffectsDX11.h
                  include/ImGuiVectorUI/Backends/RendererDX11.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Backends)
endif()
if(TARGET ImGuiVectorUIEffectsDX12)
    install(FILES include/ImGuiVectorUI/Backends/EffectsDX12.h
                  include/ImGuiVectorUI/Backends/RendererDX12.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Backends)
endif()
if(TARGET ImGuiVectorUIEffectsVulkan)
    install(FILES include/ImGuiVectorUI/Backends/EffectsVulkan.h
                  include/ImGuiVectorUI/Backends/RendererVulkan.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImGuiVectorUI/Backends)
endif()
if(TARGET ImGuiVectorUIEffectsVulkan)
    install(DIRECTORY shaders/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ImGuiVectorUI/shaders)
endif()
if(IVUI_BUNDLED_IMGUI)
    install(FILES third_party/imgui/imgui.h third_party/imgui/imconfig.h third_party/imgui/imgui_internal.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgui)
endif()
install(EXPORT ImGuiVectorUITargets
    FILE ImGuiVectorUITargets.cmake
    NAMESPACE ImGuiVectorUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImGuiVectorUI)
configure_package_config_file(cmake/ImGuiVectorUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ImGuiVectorUIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImGuiVectorUI)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/ImGuiVectorUIConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ImGuiVectorUIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ImGuiVectorUIConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImGuiVectorUI)
