if(NOT WIN32)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires Windows.")
endif()
if(NOT TARGET ImGuiVectorUI::RendererVulkan)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires IVUI_BUILD_VULKAN_EFFECTS=ON.")
endif()
if(NOT TARGET ImGuiVectorUI::PlatformWin32)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires IVUI_BUILD_WIN32_PLATFORM=ON.")
endif()

add_executable(ImGuiVectorUIDemoVulkan WIN32 main.cpp)
target_compile_features(ImGuiVectorUIDemoVulkan PRIVATE cxx_std_20)
target_compile_definitions(ImGuiVectorUIDemoVulkan PRIVATE
    UNICODE _UNICODE VK_USE_PLATFORM_WIN32_KHR)
if(MSVC)
    target_compile_options(ImGuiVectorUIDemoVulkan PRIVATE /utf-8)
endif()
target_link_libraries(ImGuiVectorUIDemoVulkan PRIVATE
    ImGuiVectorUIDemoMenu
    ImGuiVectorUI::Development
    ImGuiVectorUI::RendererVulkan
    ImGuiVectorUI::PlatformWin32
    user32 gdi32 shell32)

add_custom_command(TARGET ImGuiVectorUIDemoVulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/unifont.otf
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/unifont.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/UNIFONT-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/UNIFONT-LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/CesiumMilkTruck.glb
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/CesiumMilkTruck.glb
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../DemoMenu/assets/CESIUM-MILK-TRUCK-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/CESIUM-MILK-TRUCK-LICENSE.txt)
