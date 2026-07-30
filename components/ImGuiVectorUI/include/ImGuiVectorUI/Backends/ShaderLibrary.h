/** @file ShaderLibrary.h
 *  @brief Unified access to shaders compiled into the component library.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace IVUI
{
    enum class ShaderBackend : std::uint8_t
    {
        DirectX,
        Vulkan
    };

    struct ShaderBinary
    {
        const std::uint32_t* code = nullptr;
        std::size_t sizeBytes = 0;
    };

    /** Reads immutable shader binaries generated into ImGuiVectorUI at build time. */
    class ShaderLibrary final
    {
    public:
        static ShaderBinary Find(ShaderBackend backend, const char* filename) noexcept;

    private:
        ShaderLibrary() = delete;
        static ShaderBinary FindDirectX(const char* filename) noexcept;
        static ShaderBinary FindVulkan(const char* filename) noexcept;
    };
}
