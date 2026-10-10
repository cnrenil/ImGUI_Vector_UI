#include <ImGuiVectorUI/ImGuiVectorUI.h>
#include <ImGuiVectorUI/Backends/ShaderLibrary.h>
#include <ImGuiVectorUI/Layout/Content.h>
#include <ImGuiVectorUI/Layout/Workbench.h>
#include <ImGuiVectorUI/Theme/Design.h>

#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<IVUI::UI>);
    static_assert(!std::is_move_constructible_v<IVUI::UI>);

    IVUI::ThemeDefinition definition;
    definition.palette = IVUI::ThemePalette::Light();
    const IVUI::Theme theme = IVUI::MakeTheme(definition);
    IVUI::ContentLayoutOptions layout;
    layout.mode = IVUI::ContentLayout::Columns;
    IVUI::WorkbenchOptions workbench;
    if (theme.Text == 0 || layout.mode != IVUI::ContentLayout::Columns ||
        workbench.sidebarWidth <= 0.0f)
        return 1;

#if defined(IVUI_TEST_DIRECTX_SHADERS)
    const auto directX = IVUI::ShaderLibrary::Find(
        IVUI::ShaderBackend::DirectX, "effects_fullscreen.vs.cso");
    if (!directX.code || directX.sizeBytes == 0)
        return 1;
#endif
#if defined(IVUI_TEST_VULKAN_SHADERS)
    const auto vulkan = IVUI::ShaderLibrary::Find(
        IVUI::ShaderBackend::Vulkan, "effects.vert.spv");
    if (!vulkan.code || vulkan.sizeBytes == 0 || vulkan.code[0] != 0x07230203u)
        return 1;
#endif
    return 0;
}
