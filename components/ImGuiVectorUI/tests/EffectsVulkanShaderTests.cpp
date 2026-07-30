#include "ImGuiVectorUI/Backends/ShaderLibrary.h"

#include <array>
#include <cstdlib>

int main()
{
    constexpr std::array names = {
        "effects.vert.spv", "effects_background.frag.spv",
        "effects_blur.frag.spv", "effects_material.frag.spv",
        "effects_shape.frag.spv", "effects_bloom.frag.spv",
        "effects_composite.frag.spv"};
    for (const char* name : names)
    {
        const IVUI::ShaderBinary shader =
            IVUI::ShaderLibrary::Find(IVUI::ShaderBackend::Vulkan, name);
        if (!shader.code || shader.sizeBytes < sizeof(std::uint32_t) ||
            shader.code[0] != 0x07230203u)
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
