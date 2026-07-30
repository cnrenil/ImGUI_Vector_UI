cmake_minimum_required(VERSION 3.21)

project(ImGuiVectorUI VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(IVUI_BUILD_TESTS "Build component tests" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_BUILD_DX12_EFFECTS "Build the optional DirectX 12 effects backend" OFF)
option(IVUI_BUILD_DX11_EFFECTS "Build the optional DirectX 11 effects backend" OFF)
option(IVUI_BUILD_VULKAN_EFFECTS "Build the optional Vulkan effects backend" OFF)
option(IVUI_BUILD_WIN32_PLATFORM "Build the Win32 platform adapter" OFF)
option(IVUI_INSTALL "Generate install and package export rules" ${PROJECT_IS_TOP_LEVEL})
option(IVUI_USE_BUNDLED_IMGUI "Build ImGui from IVUI_IMGUI_SOURCE_DIR when needed" ON)

set(IVUI_IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui" CACHE PATH
    "Dear ImGui source tree used when no imgui target is supplied")

function(ivui_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
endfunction()

if(IVUI_BUILD_DX11_EFFECTS OR IVUI_BUILD_DX12_EFFECTS)
    find_program(IVUI_FXC_EXECUTABLE fxc REQUIRED)
endif()

function(ivui_add_directx_shader_target target)
    set(shader_specs
        "effects_fullscreen.vs.cso|effects.vert|main|vs_5_0"
        "effects_background.ps.cso|effects_background.frag|main|ps_5_0"
        "effects_blur.ps.cso|effects_blur.frag|main|ps_5_0"
        "effects_material.ps.cso|effects_material.frag|main|ps_5_0"
        "effects_shape.ps.cso|effects_shape.frag|main|ps_5_0"
        "effects_bloom.ps.cso|effects_bloom.frag|main|ps_5_0"
        "effects_texture.ps.cso|effects_texture.frag|main|ps_5_0"
        "effects_composite.ps.cso|effects_composite.frag|main|ps_5_0")
    set(outputs)
    set(source_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/directx")
    set(output_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/directx")
    foreach(spec IN LISTS shader_specs)
        string(REPLACE "|" ";" fields "${spec}")
        list(GET fields 0 filename)
        list(GET fields 1 source_filename)
        list(GET fields 2 entry_point)
        list(GET fields 3 profile)
        set(source "${source_directory}/${source_filename}")
        set(output "${output_directory}/${filename}")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_directory}"
            COMMAND "${IVUI_FXC_EXECUTABLE}" /nologo /Ges /O3
                    /T "${profile}" /E "${entry_point}" /Fo "${output}" "${source}"
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()
    add_custom_target(${target} DEPENDS ${outputs})
    set_target_properties(${target} PROPERTIES IVUI_SHADER_OUTPUT_DIR "${output_directory}")
    set(${target}_OUTPUTS ${outputs} PARENT_SCOPE)
endfunction()

function(ivui_embed_shader_directory function_name shader_directory shader_outputs output_name)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${output_name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND ${CMAKE_COMMAND}
            "-DIVUI_SHADER_DIRECTORY=${shader_directory}"
            "-DIVUI_OUTPUT=${output}"
            "-DIVUI_FUNCTION_NAME=${function_name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake"
        DEPENDS ${shader_outputs} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake"
        VERBATIM)
    set_source_files_properties("${output}" PROPERTIES GENERATED TRUE)
    target_sources(ImGuiVectorUI PRIVATE "${output}")
endfunction()

if(IVUI_BUILD_DX11_EFFECTS OR IVUI_BUILD_DX12_EFFECTS)
    ivui_add_directx_shader_target(ImGuiVectorUIDirectXShaders)
endif()

set(IVUI_BUNDLED_IMGUI OFF)
if(TARGET imgui::imgui AND NOT TARGET imgui)
    add_library(imgui ALIAS imgui::imgui)
endif()

if(NOT TARGET imgui)
    if(NOT IVUI_USE_BUNDLED_IMGUI)
        message(FATAL_ERROR
            "ImGuiVectorUI requires an existing imgui or imgui::imgui target, "
            "or IVUI_USE_BUNDLED_IMGUI=ON.")
    endif()
    if(NOT EXISTS "${IVUI_IMGUI_SOURCE_DIR}/imgui.cpp")
        message(FATAL_ERROR
            "Dear ImGui was not found at IVUI_IMGUI_SOURCE_DIR='${IVUI_IMGUI_SOURCE_DIR}'. "
            "Initialize the workspace submodule, set IVUI_IMGUI_SOURCE_DIR, or provide an imgui target.")
    endif()

    add_library(imgui STATIC
        ${IVUI_IMGUI_SOURCE_DIR}/imgui.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/imgui_widgets.cpp)
    target_include_directories(imgui PUBLIC
        $<BUILD_INTERFACE:${IVUI_IMGUI_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/imgui>)
    ivui_configure_target(imgui)
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
    src/Drawing/ShaderLibrary.cpp
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
    src/Widgets/Basic.cpp
    src/Widgets/Button.cpp
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
ivui_configure_target(ImGuiVectorUI)
target_compile_definitions(ImGuiVectorUI PRIVATE IMGUI_DEFINE_MATH_OPERATORS IVUI_INTERNAL_API)
target_link_libraries(ImGuiVectorUI PUBLIC imgui)
target_include_directories(ImGuiVectorUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(TARGET ImGuiVectorUIDirectXShaders)
    target_compile_definitions(ImGuiVectorUI PRIVATE IVUI_HAS_DIRECTX_SHADERS)
    get_target_property(IVUI_DIRECTX_SHADER_EMBED_DIR
        ImGuiVectorUIDirectXShaders IVUI_SHADER_OUTPUT_DIR)
    ivui_embed_shader_directory(FindDirectX
        "${IVUI_DIRECTX_SHADER_EMBED_DIR}" "${ImGuiVectorUIDirectXShaders_OUTPUTS}"
        DirectXShaders.cpp)
endif()

# Build-tree-only access for the repository demo. This target is deliberately
# omitted from package exports so downstream projects consume the public API.
add_library(ImGuiVectorUIDevelopment INTERFACE)
add_library(ImGuiVectorUI::Development ALIAS ImGuiVectorUIDevelopment)
target_link_libraries(ImGuiVectorUIDevelopment INTERFACE ImGuiVectorUI)
target_compile_definitions(ImGuiVectorUIDevelopment INTERFACE
    IMGUI_DEFINE_MATH_OPERATORS IVUI_INTERNAL_API)
target_include_directories(ImGuiVectorUIDevelopment INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(IVUI_BUILD_WIN32_PLATFORM OR IVUI_BUILD_DX11_EFFECTS OR
   IVUI_BUILD_DX12_EFFECTS OR IVUI_BUILD_VULKAN_EFFECTS)
    if(NOT EXISTS "${IVUI_IMGUI_SOURCE_DIR}/backends")
        message(FATAL_ERROR
            "Enabled platform/renderer targets require Dear ImGui backend sources at "
            "IVUI_IMGUI_SOURCE_DIR='${IVUI_IMGUI_SOURCE_DIR}'.")
    endif()
    target_include_directories(ImGuiVectorUIDevelopment INTERFACE
        ${IVUI_IMGUI_SOURCE_DIR}/backends)
endif()

if(IVUI_BUILD_DX12_EFFECTS)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_DX12_EFFECTS requires Windows.")
    endif()
    add_library(ImGuiVectorUIEffectsDX12 STATIC src/Drawing/EffectsDX12.cpp)
    add_library(ImGuiVectorUI::EffectsDX12 ALIAS ImGuiVectorUIEffectsDX12)
    set_target_properties(ImGuiVectorUIEffectsDX12 PROPERTIES EXPORT_NAME EffectsDX12)
    ivui_configure_target(ImGuiVectorUIEffectsDX12)
    target_link_libraries(ImGuiVectorUIEffectsDX12 PUBLIC ImGuiVectorUI
        PRIVATE d3d12 dxgi)
    target_include_directories(ImGuiVectorUIEffectsDX12 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    add_dependencies(ImGuiVectorUIEffectsDX12 ImGuiVectorUIDirectXShaders)
endif()

if(IVUI_BUILD_WIN32_PLATFORM)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_WIN32_PLATFORM requires Windows.")
    endif()
    add_library(ImGuiVectorUIPlatformWin32 STATIC
        src/Backends/PlatformWin32.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
    add_library(ImGuiVectorUI::PlatformWin32 ALIAS ImGuiVectorUIPlatformWin32)
    set_target_properties(ImGuiVectorUIPlatformWin32 PROPERTIES EXPORT_NAME PlatformWin32)
    ivui_configure_target(ImGuiVectorUIPlatformWin32)
    target_compile_definitions(ImGuiVectorUIPlatformWin32 PRIVATE UNICODE _UNICODE)
    target_include_directories(ImGuiVectorUIPlatformWin32
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${IVUI_IMGUI_SOURCE_DIR}/backends)
    target_link_libraries(ImGuiVectorUIPlatformWin32 PUBLIC ImGuiVectorUI PRIVATE user32)
endif()

if(IVUI_BUILD_DX11_EFFECTS)
    if(NOT WIN32)
        message(FATAL_ERROR "IVUI_BUILD_DX11_EFFECTS requires Windows.")
    endif()
    add_library(ImGuiVectorUIEffectsDX11 STATIC src/Drawing/EffectsDX11.cpp)
    add_library(ImGuiVectorUI::EffectsDX11 ALIAS ImGuiVectorUIEffectsDX11)
    set_target_properties(ImGuiVectorUIEffectsDX11 PROPERTIES EXPORT_NAME EffectsDX11)
    ivui_configure_target(ImGuiVectorUIEffectsDX11)
    target_link_libraries(ImGuiVectorUIEffectsDX11 PUBLIC ImGuiVectorUI PRIVATE d3d11)
    target_include_directories(ImGuiVectorUIEffectsDX11 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    add_dependencies(ImGuiVectorUIEffectsDX11 ImGuiVectorUIDirectXShaders)

    add_library(ImGuiVectorUIRendererDX11 STATIC
        src/Backends/RendererDX11.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
    add_library(ImGuiVectorUI::RendererDX11 ALIAS ImGuiVectorUIRendererDX11)
    set_target_properties(ImGuiVectorUIRendererDX11 PROPERTIES EXPORT_NAME RendererDX11)
    ivui_configure_target(ImGuiVectorUIRendererDX11)
    target_include_directories(ImGuiVectorUIRendererDX11
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${IVUI_IMGUI_SOURCE_DIR}/backends)
    target_link_libraries(ImGuiVectorUIRendererDX11 PUBLIC ImGuiVectorUIEffectsDX11 PRIVATE d3d11)
endif()

if(IVUI_BUILD_DX12_EFFECTS)
    add_library(ImGuiVectorUIRendererDX12 STATIC
        src/Backends/RendererDX12.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
    add_library(ImGuiVectorUI::RendererDX12 ALIAS ImGuiVectorUIRendererDX12)
    set_target_properties(ImGuiVectorUIRendererDX12 PROPERTIES EXPORT_NAME RendererDX12)
    ivui_configure_target(ImGuiVectorUIRendererDX12)
    target_include_directories(ImGuiVectorUIRendererDX12
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${IVUI_IMGUI_SOURCE_DIR}/backends)
    target_link_libraries(ImGuiVectorUIRendererDX12 PUBLIC ImGuiVectorUIEffectsDX12 PRIVATE d3d12)
endif()

if(IVUI_BUILD_VULKAN_EFFECTS)
    find_package(Vulkan REQUIRED)
    add_library(ImGuiVectorUIEffectsVulkan STATIC src/Drawing/EffectsVulkan.cpp)
    add_library(ImGuiVectorUI::EffectsVulkan ALIAS ImGuiVectorUIEffectsVulkan)
    set_target_properties(ImGuiVectorUIEffectsVulkan PROPERTIES EXPORT_NAME EffectsVulkan)
    ivui_configure_target(ImGuiVectorUIEffectsVulkan)
    target_link_libraries(ImGuiVectorUIEffectsVulkan PUBLIC ImGuiVectorUI Vulkan::Vulkan)
    target_include_directories(ImGuiVectorUIEffectsVulkan PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        find_program(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE glslangValidator REQUIRED)
    endif()
    set(IVUI_VULKAN_SHADER_OUTPUTS)
    foreach(shader effects.vert effects_background.frag effects_blur.frag effects_material.frag effects_shape.frag effects_bloom.frag effects_composite.frag)
        set(output "${CMAKE_CURRENT_BINARY_DIR}/shaders/vulkan/${shader}.spv")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/vulkan"
            COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V
                    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan/${shader}" -o "${output}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan/${shader}"
            VERBATIM)
        list(APPEND IVUI_VULKAN_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(ImGuiVectorUIVulkanShaders DEPENDS ${IVUI_VULKAN_SHADER_OUTPUTS})
    set_target_properties(ImGuiVectorUIVulkanShaders PROPERTIES
        IVUI_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/vulkan")
    add_dependencies(ImGuiVectorUIEffectsVulkan ImGuiVectorUIVulkanShaders)
    target_compile_definitions(ImGuiVectorUI PRIVATE IVUI_HAS_VULKAN_SHADERS)
    ivui_embed_shader_directory(FindVulkan
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/vulkan" "${IVUI_VULKAN_SHADER_OUTPUTS}"
        VulkanShaders.cpp)

    add_library(ImGuiVectorUIRendererVulkan STATIC
        src/Backends/RendererVulkan.cpp
        ${IVUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    add_library(ImGuiVectorUI::RendererVulkan ALIAS ImGuiVectorUIRendererVulkan)
    set_target_properties(ImGuiVectorUIRendererVulkan PROPERTIES EXPORT_NAME RendererVulkan)
    ivui_configure_target(ImGuiVectorUIRendererVulkan)
    target_include_directories(ImGuiVectorUIRendererVulkan
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${IVUI_IMGUI_SOURCE_DIR}/backends)
    target_link_libraries(ImGuiVectorUIRendererVulkan PUBLIC
        ImGuiVectorUIEffectsVulkan Vulkan::Vulkan)
endif()

if(IVUI_BUILD_TESTS)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()

    add_executable(ImGuiVectorUICoreTests tests/CoreTests.cpp)
    ivui_configure_target(ImGuiVectorUICoreTests)
    target_link_libraries(ImGuiVectorUICoreTests PRIVATE ImGuiVectorUI)
    add_test(NAME ImGuiVectorUICoreTests COMMAND ImGuiVectorUICoreTests)

    add_executable(ImGuiVectorUIEffectsProtocolTests tests/EffectsProtocolTests.cpp)
    ivui_configure_target(ImGuiVectorUIEffectsProtocolTests)
    target_link_libraries(ImGuiVectorUIEffectsProtocolTests PRIVATE ImGuiVectorUI)
    add_test(NAME ImGuiVectorUIEffectsProtocolTests COMMAND ImGuiVectorUIEffectsProtocolTests)

    if(TARGET ImGuiVectorUIEffectsDX12)
        add_executable(ImGuiVectorUIEffectsDX12Tests tests/EffectsDX12Tests.cpp)
        ivui_configure_target(ImGuiVectorUIEffectsDX12Tests)
        target_link_libraries(ImGuiVectorUIEffectsDX12Tests PRIVATE ImGuiVectorUIEffectsDX12)
        add_test(NAME ImGuiVectorUIEffectsDX12Tests COMMAND ImGuiVectorUIEffectsDX12Tests)
    endif()

    if(TARGET ImGuiVectorUIEffectsDX11)
        add_executable(ImGuiVectorUIEffectsDX11Tests tests/EffectsDX11Tests.cpp)
        ivui_configure_target(ImGuiVectorUIEffectsDX11Tests)
        target_link_libraries(ImGuiVectorUIEffectsDX11Tests PRIVATE ImGuiVectorUIEffectsDX11)
        add_test(NAME ImGuiVectorUIEffectsDX11Tests COMMAND ImGuiVectorUIEffectsDX11Tests)
    endif()

    if(TARGET ImGuiVectorUIEffectsVulkan)
        add_executable(ImGuiVectorUIEffectsVulkanShaderTests
            tests/EffectsVulkanShaderTests.cpp)
        ivui_configure_target(ImGuiVectorUIEffectsVulkanShaderTests)
        target_link_libraries(ImGuiVectorUIEffectsVulkanShaderTests PRIVATE ImGuiVectorUI)
        add_dependencies(ImGuiVectorUIEffectsVulkanShaderTests ImGuiVectorUIVulkanShaders)
        add_test(NAME ImGuiVectorUIEffectsVulkanShaderTests
            COMMAND ImGuiVectorUIEffectsVulkanShaderTests)
    endif()
endif()

if(IVUI_INSTALL)
    set(IVUI_INSTALL_TARGETS ImGuiVectorUI)
    if(IVUI_BUNDLED_IMGUI)
        list(APPEND IVUI_INSTALL_TARGETS imgui)
    endif()
    foreach(target ImGuiVectorUIEffectsDX12 ImGuiVectorUIEffectsDX11
                   ImGuiVectorUIEffectsVulkan ImGuiVectorUIPlatformWin32
                   ImGuiVectorUIRendererDX11 ImGuiVectorUIRendererDX12
                   ImGuiVectorUIRendererVulkan)
        if(TARGET ${target})
            list(APPEND IVUI_INSTALL_TARGETS ${target})
        endif()
    endforeach()

    install(TARGETS ${IVUI_INSTALL_TARGETS} EXPORT ImGuiVectorUITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    set(IVUI_PUBLIC_HEADERS
        include/ImGuiVectorUI/ImGuiVectorUI.h
        include/ImGuiVectorUI/UI.h
        include/ImGuiVectorUI/Backends/ShaderLibrary.h
        include/ImGuiVectorUI/Core/Types.h
        include/ImGuiVectorUI/Core/Easing.h
        include/ImGuiVectorUI/Drawing/Effects.h
        include/ImGuiVectorUI/Drawing/Icons.h
        include/ImGuiVectorUI/Layout/Flex.h
        include/ImGuiVectorUI/Layout/Panel.h
        include/ImGuiVectorUI/Layout/StatusChips.h
        include/ImGuiVectorUI/Theme/Theme.h
        include/ImGuiVectorUI/Widgets/Keybind.h
        include/ImGuiVectorUI/Widgets/Toast.h)
    foreach(header IN LISTS IVUI_PUBLIC_HEADERS)
        get_filename_component(header_directory "${header}" DIRECTORY)
        file(RELATIVE_PATH install_directory "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/${header_directory}")
        install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${install_directory}")
    endforeach()

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
    if(IVUI_BUNDLED_IMGUI)
        install(FILES ${IVUI_IMGUI_SOURCE_DIR}/imgui.h
                      ${IVUI_IMGUI_SOURCE_DIR}/imconfig.h
                      ${IVUI_IMGUI_SOURCE_DIR}/imgui_internal.h
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
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/ImGuiVectorUI)
endif()
