#include "ImGuiVectorUI/Optional/Gltf.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "Check failed: %s\n", message);
            std::exit(EXIT_FAILURE);
        }
    }

    void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data)
    {
        std::ofstream file(path, std::ios::binary);
        Check(static_cast<bool>(file), "open binary test file");
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
        Check(static_cast<bool>(file), "write binary test file");
    }

    void AppendFloat(std::vector<std::uint8_t>& data, float value)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        data.insert(data.end(), bytes, bytes + sizeof(value));
    }

    void AppendUInt16(std::vector<std::uint8_t>& data, std::uint16_t value)
    {
        data.push_back(static_cast<std::uint8_t>(value & 0xffu));
        data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "ivui-gltf-phase5-test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    Check(!error, "create temporary directory");

    std::vector<std::uint8_t> binary;
    binary.reserve(102);
    const float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f};
    const float normals[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.0f, 1.0f};
    const float texCoords[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    for (float value : positions) AppendFloat(binary, value);
    for (float value : normals) AppendFloat(binary, value);
    for (float value : texCoords) AppendFloat(binary, value);
    AppendUInt16(binary, 0);
    AppendUInt16(binary, 1);
    AppendUInt16(binary, 2);
    Check(binary.size() == 102, "construct test buffer");
    WriteFile(directory / "mesh.bin", binary);

    const std::vector<std::uint8_t> ppm = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n',
        255, 64, 32};
    WriteFile(directory / "albedo.ppm", ppm);

    const std::string json = R"JSON({
"asset":{"version":"2.0"},"scene":0,
"scenes":[{"name":"Test Scene","nodes":[0]}],
"nodes":[{"name":"Translated","mesh":0,"translation":[2,3,4]}],
"meshes":[{"name":"Triangle","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],
"materials":[{"name":"Paint","pbrMetallicRoughness":{"baseColorFactor":[0.8,0.7,0.6,1.0],"metallicFactor":0.25,"roughnessFactor":0.75,"baseColorTexture":{"index":0}},"normalTexture":{"index":0},"doubleSided":true}],
"textures":[{"name":"Albedo","sampler":0,"source":0}],
"samplers":[{"name":"Repeat","wrapS":10497,"wrapT":10497}],
"images":[{"name":"Pixel","uri":"albedo.ppm"}],
"buffers":[{"uri":"mesh.bin","byteLength":102}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}]
})JSON";
    {
        std::ofstream file(directory / "scene.gltf", std::ios::binary);
        Check(static_cast<bool>(file), "open JSON test file");
        file << json;
    }

    IVUI::GltfScene scene;
    std::string loadError;
    Check(IVUI::LoadGltf((directory / "scene.gltf").string().c_str(), scene, &loadError),
          loadError.empty() ? "load test glTF" : loadError.c_str());
    Check(scene.name == "Test Scene", "scene name");
    Check(scene.meshes.size() == 1, "mesh count");
    Check(scene.meshes[0].name == "Triangle", "mesh name");
    Check(scene.meshes[0].hasNormals && scene.meshes[0].hasTexCoords,
          "vertex attributes");
    Check(scene.meshes[0].materialIndex == 0, "material index");
    Check(scene.meshes[0].vertices[0].position.x == 2.0f &&
              scene.meshes[0].vertices[0].position.y == 3.0f &&
              scene.meshes[0].vertices[0].position.z == 4.0f,
          "node transform");
    Check(scene.meshes[0].vertices[1].texCoord.x == 1.0f &&
              scene.meshes[0].vertices[2].texCoord.y == 1.0f,
          "texture coordinates");
    Check(scene.materials.size() == 1 && scene.materials[0].name == "Paint",
          "material metadata");
    Check(scene.materials[0].baseColorTexture.textureIndex == 0 &&
              scene.materials[0].normalTexture.textureIndex == 0 &&
              scene.materials[0].doubleSided,
          "material textures");
    Check(scene.textures.size() == 1 && scene.textures[0].imageIndex == 0,
          "texture metadata");
    Check(scene.images.size() == 1 && scene.images[0].Decoded() &&
              scene.images[0].pixels[0] == 255 && scene.images[0].pixels[1] == 64,
          "decoded image");
    Check(std::fabs(scene.boundsMin.x - 2.0f) < 0.001f &&
              std::fabs(scene.boundsMax.y - 4.0f) < 0.001f &&
              std::fabs(scene.boundsMax.z - 4.0f) < 0.001f,
          "scene bounds");

    if (argc > 1)
    {
        IVUI::GltfScene externalScene;
        std::string externalError;
        Check(IVUI::LoadGltf(argv[1], externalScene, &externalError),
              externalError.empty() ? "load external sample model" : externalError.c_str());
        Check(!externalScene.Empty() && !externalScene.materials.empty(),
              "external sample model geometry and material");
    }

    std::filesystem::remove_all(directory, error);
    return EXIT_SUCCESS;
}
