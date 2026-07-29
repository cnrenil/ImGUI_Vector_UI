if(NOT WIN32)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires Windows.")
endif()
if(NOT TARGET ImGuiVectorUI::RendererVulkan)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires IVUI_BUILD_VULKAN_EFFECTS=ON.")
endif()
if(NOT TARGET ImGuiVectorUI::PlatformWin32)
    message(FATAL_ERROR "IVUI_BUILD_VULKAN_DEMO requires IVUI_BUILD_WIN32_PLATFORM=ON.")
endif()

add_executable(ImGuiVectorUIDemoVulkan WIN32
    main.cpp
    ../Dx12Host/DemoMenu.cpp)
target_compile_features(ImGuiVectorUIDemoVulkan PRIVATE cxx_std_20)
target_compile_definitions(ImGuiVectorUIDemoVulkan PRIVATE
    UNICODE _UNICODE VK_USE_PLATFORM_WIN32_KHR)
target_include_directories(ImGuiVectorUIDemoVulkan PRIVATE ../Dx12Host)
if(MSVC)
    target_compile_options(ImGuiVectorUIDemoVulkan PRIVATE /utf-8)
endif()
target_link_libraries(ImGuiVectorUIDemoVulkan PRIVATE
    ImGuiVectorUI::Development
    ImGuiVectorUI::RendererVulkan
    ImGuiVectorUI::PlatformWin32
    user32 gdi32 shell32)

add_dependencies(ImGuiVectorUIDemoVulkan ImGuiVectorUIVulkanShaders)
get_target_property(IVUI_VULKAN_SHADER_DIR ImGuiVectorUIVulkanShaders IVUI_SHADER_OUTPUT_DIR)
add_custom_command(TARGET ImGuiVectorUIDemoVulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dx12Host/assets/unifont.otf
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/unifont.otf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dx12Host/assets/UNIFONT-LICENSE.txt
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/UNIFONT-LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${IVUI_VULKAN_SHADER_DIR}
        $<TARGET_FILE_DIR:ImGuiVectorUIDemoVulkan>/shaders)
