add_library(ImGuiVectorUIDemoMenu STATIC DemoMenu.cpp LayoutGallery.cpp)
target_compile_features(ImGuiVectorUIDemoMenu PRIVATE cxx_std_20)
target_include_directories(ImGuiVectorUIDemoMenu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ImGuiVectorUIDemoMenu PRIVATE ImGuiVectorUI::Development)
target_link_libraries(ImGuiVectorUIDemoMenu PRIVATE
    ImGuiVectorUI::Charts
    ImGuiVectorUI::ModelViewer)
if(MSVC)
    target_compile_options(ImGuiVectorUIDemoMenu PRIVATE /utf-8)
endif()
