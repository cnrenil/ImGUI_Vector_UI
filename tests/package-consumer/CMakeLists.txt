cmake_minimum_required(VERSION 3.21)
project(ImGuiVectorUIPackageConsumer LANGUAGES CXX)

find_package(ImGuiVectorUI 1 CONFIG REQUIRED)
add_executable(package-consumer main.cpp)
target_compile_features(package-consumer PRIVATE cxx_std_20)
target_link_libraries(package-consumer PRIVATE ImGuiVectorUI::ImGuiVectorUI)

if(TARGET ImGuiVectorUI::EffectsDX11 OR TARGET ImGuiVectorUI::EffectsDX12)
    target_compile_definitions(package-consumer PRIVATE IVUI_TEST_DIRECTX_SHADERS)
endif()
if(TARGET ImGuiVectorUI::EffectsVulkan)
    target_compile_definitions(package-consumer PRIVATE IVUI_TEST_VULKAN_SHADERS)
endif()
add_custom_command(TARGET package-consumer POST_BUILD
    COMMAND $<TARGET_FILE:package-consumer>
    VERBATIM)
