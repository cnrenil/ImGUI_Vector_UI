if(NOT WIN32)
    message(FATAL_ERROR "IVUI_BUILD_DX11_DEMO requires Windows.")
endif()
if(NOT TARGET ImGuiVectorUI::RendererDX11)
    message(FATAL_ERROR "IVUI_BUILD_DX11_DEMO requires IVUI_BUILD_DX11_EFFECTS=ON.")
endif()
if(NOT TARGET ImGuiVectorUI::PlatformWin32)
    message(FATAL_ERROR "IVUI_BUILD_DX11_DEMO requires IVUI_BUILD_WIN32_PLATFORM=ON.")
endif()

add_executable(ImGuiVectorUIDemoDX11 WIN32 main.cpp)
target_compile_features(ImGuiVectorUIDemoDX11 PRIVATE cxx_std_20)
target_compile_definitions(ImGuiVectorUIDemoDX11 PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(ImGuiVectorUIDemoDX11 PRIVATE /utf-8)
endif()
target_link_libraries(ImGuiVectorUIDemoDX11 PRIVATE
    ImGuiVectorUIDemoMenu
    ImGuiVectorUI::Development
    ImGuiVectorUI::RendererDX11
    ImGuiVectorUI::PlatformWin32
    d3d11 dxgi user32 gdi32 shell32)

add_custom_command(TARGET ImGuiVectorUIDemoDX11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/unifont.otf
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoDX11>/unifont.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/UNIFONT-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoDX11>/UNIFONT-LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/CesiumMilkTruck.glb
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoDX11>/CesiumMilkTruck.glb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/CESIUM-MILK-TRUCK-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoDX11>/CESIUM-MILK-TRUCK-LICENSE.txt)
