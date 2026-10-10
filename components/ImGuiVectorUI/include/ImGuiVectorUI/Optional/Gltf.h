/** @file Gltf.h @brief glTF/GLB CPU scene loader. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace IVUI
{
    struct GltfVec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct GltfVec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct GltfVec4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;
    };

    struct GltfVertex
    {
        GltfVec3 position{};
        GltfVec3 normal{0.0f, 0.0f, 1.0f};
        GltfVec2 texCoord{};
    };

    enum class GltfPrimitiveType
    {
        Points,
        Lines,
        LineLoop,
        LineStrip,
        Triangles,
        TriangleStrip,
        TriangleFan
    };

    struct GltfMesh
    {
        std::vector<GltfVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::string name;
        int materialIndex = -1;
        GltfPrimitiveType primitiveType = GltfPrimitiveType::Triangles;
        bool hasNormals = false;
        bool hasTexCoords = false;
    };

    enum class GltfAlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    struct GltfTextureInfo
    {
        int textureIndex = -1;
        int texCoord = 0;
        float scale = 1.0f;
    };

    struct GltfMaterial
    {
        std::string name;
        GltfVec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        GltfTextureInfo baseColorTexture{};
        GltfTextureInfo metallicRoughnessTexture{};
        GltfTextureInfo normalTexture{};
        GltfTextureInfo occlusionTexture{};
        GltfTextureInfo emissiveTexture{};
        GltfVec3 emissiveFactor{};
        GltfAlphaMode alphaMode = GltfAlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;
        bool unlit = false;
        bool hasPbrMetallicRoughness = false;
    };

    struct GltfImage
    {
        std::string name;
        std::string uri;
        std::string mimeType;
        int width = 0;
        int height = 0;
        int sourceChannels = 0;
        std::vector<std::uint8_t> pixels;

        bool Decoded() const noexcept
        {
            return width > 0 && height > 0 && pixels.size() ==
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        }
    };

    struct GltfSampler
    {
        std::string name;
        int magFilter = 0;
        int minFilter = 0;
        int wrapS = 10497;
        int wrapT = 10497;
    };

    struct GltfTexture
    {
        std::string name;
        int imageIndex = -1;
        int samplerIndex = -1;
    };

    struct GltfScene
    {
        std::vector<GltfMesh> meshes;
        GltfVec3 boundsMin{};
        GltfVec3 boundsMax{};
        std::string name;
        std::vector<GltfMaterial> materials;
        std::vector<GltfTexture> textures;
        std::vector<GltfImage> images;
        std::vector<GltfSampler> samplers;

        bool Empty() const noexcept { return meshes.empty(); }
    };

    /**
     * Loads static mesh, material and image data from .gltf or .glb into IVUI-owned memory.
     * Images are decoded to tightly packed RGBA8 when stb_image can decode them; an image
     * decode failure does not discard otherwise valid geometry and material metadata.
     */
    bool LoadGltf(const char* path, GltfScene& scene, std::string* error = nullptr);
}
