#include "ImGuiVectorUI/Optional/Gltf.h"

#include "cgltf.h"
#include "stb_image.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace IVUI
{
    namespace
    {
        void Error(std::string* output, const char* message)
        {
            if (output)
                *output = message ? message : "glTF load failed";
        }

        std::string StringOrEmpty(const char* value)
        {
            return value ? std::string(value) : std::string();
        }

        struct Matrix4
        {
            float values[16] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f};
        };

        Matrix4 FromLocalNode(const cgltf_node* node)
        {
            Matrix4 result;
            cgltf_node_transform_local(node, result.values);
            return result;
        }

        Matrix4 Multiply(const Matrix4& left, const Matrix4& right)
        {
            Matrix4 result{};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    result.values[column * 4 + row] =
                        left.values[0 * 4 + row] * right.values[column * 4 + 0] +
                        left.values[1 * 4 + row] * right.values[column * 4 + 1] +
                        left.values[2 * 4 + row] * right.values[column * 4 + 2] +
                        left.values[3 * 4 + row] * right.values[column * 4 + 3];
            return result;
        }

        Matrix4 WorldTransform(const cgltf_node* node)
        {
            if (!node)
                return {};
            const Matrix4 local = FromLocalNode(node);
            return node->parent ? Multiply(WorldTransform(node->parent), local) : local;
        }

        GltfVec3 Normalize(GltfVec3 value)
        {
            const float length = std::sqrt(value.x * value.x + value.y * value.y +
                                           value.z * value.z);
            if (length <= 0.000001f)
                return {0.0f, 0.0f, 1.0f};
            return {value.x / length, value.y / length, value.z / length};
        }

        GltfVec3 TransformPoint(const Matrix4& matrix, GltfVec3 value)
        {
            return {
                matrix.values[0] * value.x + matrix.values[4] * value.y +
                    matrix.values[8] * value.z + matrix.values[12],
                matrix.values[1] * value.x + matrix.values[5] * value.y +
                    matrix.values[9] * value.z + matrix.values[13],
                matrix.values[2] * value.x + matrix.values[6] * value.y +
                    matrix.values[10] * value.z + matrix.values[14]};
        }

        GltfVec3 TransformNormal(const Matrix4& matrix, GltfVec3 value)
        {
            const float a = matrix.values[0];
            const float b = matrix.values[4];
            const float c = matrix.values[8];
            const float d = matrix.values[1];
            const float e = matrix.values[5];
            const float f = matrix.values[9];
            const float g = matrix.values[2];
            const float h = matrix.values[6];
            const float i = matrix.values[10];
            const float determinant = a * (e * i - f * h) -
                                      b * (d * i - f * g) +
                                      c * (d * h - e * g);
            if (std::fabs(determinant) <= 0.000001f)
                return Normalize({
                    a * value.x + b * value.y + c * value.z,
                    d * value.x + e * value.y + f * value.z,
                    g * value.x + h * value.y + i * value.z});

            const float inverseDeterminant = 1.0f / determinant;
            return Normalize({
                ((e * i - f * h) * value.x + (f * g - d * i) * value.y +
                 (d * h - e * g) * value.z) * inverseDeterminant,
                ((c * h - b * i) * value.x + (a * i - c * g) * value.y +
                 (b * g - a * h) * value.z) * inverseDeterminant,
                ((b * f - c * e) * value.x + (c * d - a * f) * value.y +
                 (a * e - b * d) * value.z) * inverseDeterminant});
        }

        const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive,
                                            cgltf_attribute_type type, int index = 0)
        {
            for (cgltf_size attributeIndex = 0;
                 attributeIndex < primitive.attributes_count; ++attributeIndex)
            {
                const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
                if (attribute.type == type && attribute.index == index)
                    return attribute.data;
            }
            return nullptr;
        }

        GltfPrimitiveType PrimitiveType(cgltf_primitive_type type)
        {
            switch (type)
            {
            case cgltf_primitive_type_points: return GltfPrimitiveType::Points;
            case cgltf_primitive_type_lines: return GltfPrimitiveType::Lines;
            case cgltf_primitive_type_line_loop: return GltfPrimitiveType::LineLoop;
            case cgltf_primitive_type_line_strip: return GltfPrimitiveType::LineStrip;
            case cgltf_primitive_type_triangle_strip: return GltfPrimitiveType::TriangleStrip;
            case cgltf_primitive_type_triangle_fan: return GltfPrimitiveType::TriangleFan;
            case cgltf_primitive_type_triangles:
            case cgltf_primitive_type_invalid:
            default: return GltfPrimitiveType::Triangles;
            }
        }

        bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                return false;
            const std::streamoff end = file.tellg();
            if (end < 0 || static_cast<unsigned long long>(end) >
                               static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
                return false;
            output.resize(static_cast<std::size_t>(end));
            file.seekg(0, std::ios::beg);
            if (!output.empty())
                file.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
            return file.good() || file.eof();
        }

        int Base64Value(char value)
        {
            if (value >= 'A' && value <= 'Z')
                return value - 'A';
            if (value >= 'a' && value <= 'z')
                return value - 'a' + 26;
            if (value >= '0' && value <= '9')
                return value - '0' + 52;
            if (value == '+')
                return 62;
            if (value == '/')
                return 63;
            return -1;
        }

        bool DecodeBase64(std::string_view input, std::vector<std::uint8_t>& output)
        {
            output.clear();
            int value = 0;
            int bits = -8;
            for (const char character : input)
            {
                if (std::isspace(static_cast<unsigned char>(character)))
                    continue;
                if (character == '=')
                    break;
                const int decoded = Base64Value(character);
                if (decoded < 0)
                    return false;
                value = (value << 6) | decoded;
                bits += 6;
                if (bits >= 0)
                {
                    output.push_back(static_cast<std::uint8_t>((value >> bits) & 0xff));
                    bits -= 8;
                }
            }
            return !output.empty();
        }

        bool DecodePercentBytes(std::string_view input, std::vector<std::uint8_t>& output)
        {
            output.clear();
            output.reserve(input.size());
            for (std::size_t index = 0; index < input.size(); ++index)
            {
                if (input[index] == '%' && index + 2 < input.size())
                {
                    const auto hex = [](char value) -> int {
                        if (value >= '0' && value <= '9') return value - '0';
                        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
                        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
                        return -1;
                    };
                    const int hexHigh = hex(input[index + 1]);
                    const int hexLow = hex(input[index + 2]);
                    if (hexHigh >= 0 && hexLow >= 0)
                    {
                        output.push_back(static_cast<std::uint8_t>((hexHigh << 4) | hexLow));
                        index += 2;
                        continue;
                    }
                }
                output.push_back(static_cast<std::uint8_t>(input[index]));
            }
            return !output.empty();
        }

        bool DecodeDataUri(std::string_view uri, std::vector<std::uint8_t>& output)
        {
            if (uri.size() < 5 || uri.substr(0, 5) != "data:")
                return false;
            const std::size_t comma = uri.find(',');
            if (comma == std::string_view::npos)
                return false;
            const std::string_view header = uri.substr(5, comma - 5);
            const std::string_view payload = uri.substr(comma + 1);
            const bool base64 = header.find(";base64") != std::string_view::npos;
            return base64 ? DecodeBase64(payload, output) : DecodePercentBytes(payload, output);
        }

        bool ImageBytes(const cgltf_image& source, const std::filesystem::path& basePath,
                        std::vector<std::uint8_t>& owned, const std::uint8_t*& bytes,
                        std::size_t& byteCount)
        {
            bytes = nullptr;
            byteCount = 0;
            owned.clear();
            if (source.buffer_view)
            {
                bytes = cgltf_buffer_view_data(source.buffer_view);
                byteCount = source.buffer_view->size;
                return bytes != nullptr && byteCount != 0;
            }
            if (!source.uri)
                return false;

            const std::string uri(source.uri);
            if (uri.rfind("data:", 0) == 0)
            {
                if (!DecodeDataUri(uri, owned))
                    return false;
            }
            else
            {
                std::string decoded = uri;
                cgltf_decode_uri(decoded.data());
                if (decoded.find("://") != std::string::npos ||
                    !ReadFile(basePath / std::filesystem::path(decoded), owned))
                    return false;
            }
            bytes = owned.data();
            byteCount = owned.size();
            return bytes != nullptr && byteCount != 0;
        }

        void DecodeImage(const cgltf_image& source, const std::filesystem::path& basePath,
                         GltfImage& destination)
        {
            std::vector<std::uint8_t> owned;
            const std::uint8_t* bytes = nullptr;
            std::size_t byteCount = 0;
            if (!ImageBytes(source, basePath, owned, bytes, byteCount) ||
                byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                return;

            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* decoded = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(bytes), static_cast<int>(byteCount),
                &width, &height, &channels, 4);
            if (!decoded || width <= 0 || height <= 0)
            {
                if (decoded)
                    stbi_image_free(decoded);
                return;
            }

            const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                           static_cast<std::size_t>(height) * 4u;
            destination.width = width;
            destination.height = height;
            destination.sourceChannels = channels;
            destination.pixels.assign(decoded, decoded + pixelCount);
            stbi_image_free(decoded);
        }

        GltfTextureInfo TextureInfo(const cgltf_texture_view& source, const cgltf_data* data)
        {
            GltfTextureInfo result;
            if (source.texture)
                result.textureIndex = static_cast<int>(cgltf_texture_index(data, source.texture));
            result.texCoord = source.texcoord >= 0 ? source.texcoord : 0;
            result.scale = source.scale;
            return result;
        }

        GltfMaterial Material(const cgltf_material& source, const cgltf_data* data)
        {
            GltfMaterial result;
            result.name = StringOrEmpty(source.name);
            result.hasPbrMetallicRoughness = source.has_pbr_metallic_roughness != 0;
            if (source.has_pbr_metallic_roughness)
            {
                result.baseColorFactor = {
                    source.pbr_metallic_roughness.base_color_factor[0],
                    source.pbr_metallic_roughness.base_color_factor[1],
                    source.pbr_metallic_roughness.base_color_factor[2],
                    source.pbr_metallic_roughness.base_color_factor[3]};
                result.metallicFactor = source.pbr_metallic_roughness.metallic_factor;
                result.roughnessFactor = source.pbr_metallic_roughness.roughness_factor;
                result.baseColorTexture = TextureInfo(
                    source.pbr_metallic_roughness.base_color_texture, data);
                result.metallicRoughnessTexture = TextureInfo(
                    source.pbr_metallic_roughness.metallic_roughness_texture, data);
            }
            result.normalTexture = TextureInfo(source.normal_texture, data);
            result.occlusionTexture = TextureInfo(source.occlusion_texture, data);
            result.emissiveTexture = TextureInfo(source.emissive_texture, data);
            result.emissiveFactor = {
                source.emissive_factor[0], source.emissive_factor[1], source.emissive_factor[2]};
            result.alphaMode = source.alpha_mode == cgltf_alpha_mode_mask ?
                GltfAlphaMode::Mask : source.alpha_mode == cgltf_alpha_mode_blend ?
                GltfAlphaMode::Blend : GltfAlphaMode::Opaque;
            result.alphaCutoff = source.alpha_cutoff;
            result.doubleSided = source.double_sided != 0;
            result.unlit = source.unlit != 0;
            return result;
        }

        void CollectInstances(const cgltf_node* node,
                              std::vector<std::pair<const cgltf_mesh*, const cgltf_node*>>& output)
        {
            if (!node)
                return;
            if (node->mesh)
                output.emplace_back(node->mesh, node);
            for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex)
                CollectInstances(node->children[childIndex], output);
        }

        void UpdateBounds(GltfScene& scene, GltfVec3 position)
        {
            scene.boundsMin.x = (std::min)(scene.boundsMin.x, position.x);
            scene.boundsMin.y = (std::min)(scene.boundsMin.y, position.y);
            scene.boundsMin.z = (std::min)(scene.boundsMin.z, position.z);
            scene.boundsMax.x = (std::max)(scene.boundsMax.x, position.x);
            scene.boundsMax.y = (std::max)(scene.boundsMax.y, position.y);
            scene.boundsMax.z = (std::max)(scene.boundsMax.z, position.z);
        }

        void AppendPrimitive(const cgltf_mesh& sourceMesh, const cgltf_primitive& source,
                             const cgltf_node* node, const cgltf_data* data, GltfScene& scene)
        {
            const cgltf_accessor* positions = FindAttribute(source, cgltf_attribute_type_position);
            if (!positions || positions->count == 0)
                return;

            const cgltf_accessor* normals = FindAttribute(source, cgltf_attribute_type_normal);
            const cgltf_accessor* texCoords = FindAttribute(source, cgltf_attribute_type_texcoord);
            const Matrix4 world = WorldTransform(node);

            GltfMesh mesh;
            mesh.name = StringOrEmpty(sourceMesh.name);
            if (mesh.name.empty() && node && node->name)
                mesh.name = node->name;
            mesh.materialIndex = source.material ?
                static_cast<int>(cgltf_material_index(data, source.material)) : -1;
            mesh.primitiveType = PrimitiveType(source.type);
            mesh.hasNormals = normals != nullptr && normals->count >= positions->count;
            mesh.hasTexCoords = texCoords != nullptr && texCoords->count >= positions->count;
            mesh.vertices.resize(positions->count);

            for (cgltf_size vertexIndex = 0; vertexIndex < positions->count; ++vertexIndex)
            {
                float value[3] = {};
                if (cgltf_accessor_read_float(positions, vertexIndex, value, 3))
                {
                    mesh.vertices[vertexIndex].position = TransformPoint(
                        world, GltfVec3{value[0], value[1], value[2]});
                    UpdateBounds(scene, mesh.vertices[vertexIndex].position);
                }

                if (normals && vertexIndex < normals->count &&
                    cgltf_accessor_read_float(normals, vertexIndex, value, 3))
                    mesh.vertices[vertexIndex].normal = TransformNormal(
                        world, GltfVec3{value[0], value[1], value[2]});

                float uv[2] = {};
                if (texCoords && vertexIndex < texCoords->count &&
                    cgltf_accessor_read_float(texCoords, vertexIndex, uv, 2))
                    mesh.vertices[vertexIndex].texCoord = {uv[0], uv[1]};
            }

            if (source.indices)
            {
                mesh.indices.resize(source.indices->count);
                for (cgltf_size index = 0; index < source.indices->count; ++index)
                    mesh.indices[index] = static_cast<std::uint32_t>(
                        cgltf_accessor_read_index(source.indices, index));
            }
            else
            {
                mesh.indices.resize(mesh.vertices.size());
                for (std::size_t index = 0; index < mesh.vertices.size(); ++index)
                    mesh.indices[index] = static_cast<std::uint32_t>(index);
            }
            scene.meshes.push_back(std::move(mesh));
        }
    }

    bool LoadGltf(const char* path, GltfScene& scene, std::string* error)
    {
        scene = {};
        if (!path || !*path)
        {
            Error(error, "glTF path is empty");
            return false;
        }

        cgltf_options options{};
        cgltf_data* data = nullptr;
        if (cgltf_parse_file(&options, path, &data) != cgltf_result_success || !data)
        {
            Error(error, "Unable to parse glTF or GLB file");
            return false;
        }

        const cgltf_result buffers = cgltf_load_buffers(&options, data, path);
        if (buffers != cgltf_result_success)
        {
            cgltf_free(data);
            Error(error, "Unable to load glTF buffers");
            return false;
        }

        if (data->scene && data->scene->name)
            scene.name = data->scene->name;
        scene.boundsMin = GltfVec3{FLT_MAX, FLT_MAX, FLT_MAX};
        scene.boundsMax = GltfVec3{-FLT_MAX, -FLT_MAX, -FLT_MAX};

        scene.materials.reserve(data->materials_count);
        for (cgltf_size materialIndex = 0; materialIndex < data->materials_count; ++materialIndex)
            scene.materials.push_back(Material(data->materials[materialIndex], data));

        scene.samplers.reserve(data->samplers_count);
        for (cgltf_size samplerIndex = 0; samplerIndex < data->samplers_count; ++samplerIndex)
        {
            const cgltf_sampler& source = data->samplers[samplerIndex];
            GltfSampler sampler;
            sampler.name = StringOrEmpty(source.name);
            sampler.magFilter = static_cast<int>(source.mag_filter);
            sampler.minFilter = static_cast<int>(source.min_filter);
            sampler.wrapS = static_cast<int>(source.wrap_s);
            sampler.wrapT = static_cast<int>(source.wrap_t);
            scene.samplers.push_back(std::move(sampler));
        }

        scene.textures.reserve(data->textures_count);
        for (cgltf_size textureIndex = 0; textureIndex < data->textures_count; ++textureIndex)
        {
            const cgltf_texture& source = data->textures[textureIndex];
            GltfTexture texture;
            texture.name = StringOrEmpty(source.name);
            if (source.image)
                texture.imageIndex = static_cast<int>(cgltf_image_index(data, source.image));
            if (source.sampler)
                texture.samplerIndex = static_cast<int>(cgltf_sampler_index(data, source.sampler));
            scene.textures.push_back(std::move(texture));
        }

        const std::filesystem::path basePath = std::filesystem::path(path).parent_path();
        scene.images.reserve(data->images_count);
        for (cgltf_size imageIndex = 0; imageIndex < data->images_count; ++imageIndex)
        {
            const cgltf_image& source = data->images[imageIndex];
            GltfImage image;
            image.name = StringOrEmpty(source.name);
            image.uri = StringOrEmpty(source.uri);
            image.mimeType = StringOrEmpty(source.mime_type);
            DecodeImage(source, basePath, image);
            scene.images.push_back(std::move(image));
        }

        std::vector<std::pair<const cgltf_mesh*, const cgltf_node*>> instances;
        if (data->scene)
        {
            for (cgltf_size nodeIndex = 0; nodeIndex < data->scene->nodes_count; ++nodeIndex)
                CollectInstances(data->scene->nodes[nodeIndex], instances);
        }
        if (instances.empty())
        {
            for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
                instances.emplace_back(&data->meshes[meshIndex], nullptr);
        }

        for (const auto& instance : instances)
        {
            const cgltf_mesh& sourceMesh = *instance.first;
            for (cgltf_size primitiveIndex = 0;
                 primitiveIndex < sourceMesh.primitives_count; ++primitiveIndex)
                AppendPrimitive(sourceMesh, sourceMesh.primitives[primitiveIndex],
                                instance.second, data, scene);
        }

        cgltf_free(data);
        if (scene.Empty())
        {
            Error(error, "glTF contains no POSITION mesh primitives");
            return false;
        }
        if (error)
            error->clear();
        return true;
    }
}
