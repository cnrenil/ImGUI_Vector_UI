if(NOT WIN32)
    message(FATAL_ERROR "IVUI_BUILD_DEMO requires Windows.")
endif()
if(NOT TARGET ImGuiVectorUI::RendererDX12)
    message(FATAL_ERROR "IVUI_BUILD_DEMO requires IVUI_BUILD_DX12_EFFECTS=ON.")
endif()
if(NOT TARGET ImGuiVectorUI::PlatformWin32)
    message(FATAL_ERROR "IVUI_BUILD_DEMO requires IVUI_BUILD_WIN32_PLATFORM=ON.")
endif()

add_executable(ImGuiVectorUIDemo WIN32 main.cpp)
target_compile_features(ImGuiVectorUIDemo PRIVATE cxx_std_20)
target_compile_definitions(ImGuiVectorUIDemo PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(ImGuiVectorUIDemo PRIVATE /utf-8)
endif()
target_link_libraries(ImGuiVectorUIDemo PRIVATE
    ImGuiVectorUIDemoMenu
    ImGuiVectorUI::Development
    ImGuiVectorUI::RendererDX12
    ImGuiVectorUI::PlatformWin32
    d3d12 dxgi dxguid user32 gdi32 shell32)

add_custom_command(TARGET ImGuiVectorUIDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/unifont.otf
        $<TARGET_FILE_DIR:ImGuiVectorUIDemo>/unifont.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/UNIFONT-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemo>/UNIFONT-LICENSE.txt)

if(IVUI_BUILD_TESTS)
    add_executable(ImGuiVectorUIFontAssetTests FontAssetTests.cpp)
    target_compile_features(ImGuiVectorUIFontAssetTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(ImGuiVectorUIFontAssetTests PRIVATE /utf-8)
    endif()
    target_link_libraries(ImGuiVectorUIFontAssetTests PRIVATE imgui)
    add_test(NAME ImGuiVectorUIFontAssetTests COMMAND ImGuiVectorUIFontAssetTests
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/unifont.otf)
endif()
