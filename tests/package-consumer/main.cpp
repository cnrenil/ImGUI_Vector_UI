#include <ImGuiVectorUI/ImGuiVectorUI.h>
#include <ImGuiVectorUI/Backends/ShaderLibrary.h>

#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<IVUI::UI>);
    static_assert(!std::is_move_constructible_v<IVUI::UI>);

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
