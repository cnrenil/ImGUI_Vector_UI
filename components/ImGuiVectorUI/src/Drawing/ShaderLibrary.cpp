#include "ImGuiVectorUI/Backends/ShaderLibrary.h"

namespace IVUI
{
    ShaderBinary ShaderLibrary::Find(ShaderBackend backend, const char* filename) noexcept
    {
        switch (backend)
        {
        case ShaderBackend::DirectX:
#if defined(IVUI_HAS_DIRECTX_SHADERS)
            return FindDirectX(filename);
#else
            return {};
#endif
        case ShaderBackend::Vulkan:
#if defined(IVUI_HAS_VULKAN_SHADERS)
            return FindVulkan(filename);
#else
            return {};
#endif
        default:
            return {};
        }
    }
}
