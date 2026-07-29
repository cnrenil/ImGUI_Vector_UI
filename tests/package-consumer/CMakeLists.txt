cmake_minimum_required(VERSION 3.21)
project(ImGuiVectorUIPackageConsumer LANGUAGES CXX)

find_package(ImGuiVectorUI 1 CONFIG REQUIRED)
add_executable(package-consumer main.cpp)
target_compile_features(package-consumer PRIVATE cxx_std_20)
target_link_libraries(package-consumer PRIVATE ImGuiVectorUI::ImGuiVectorUI)
