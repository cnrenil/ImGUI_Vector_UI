#include "ImGuiVectorUI/Optional/ModelViewer.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"
#include "glm/glm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace IVUI
{
    namespace
    {
        constexpr float kEpsilon = 0.0001f;

        struct ProjectedVertex
        {
            ImVec2 position{};
            float depth = 0.0f;
            GltfVec3 viewPosition{};
            GltfVec3 viewNormal{};
            GltfVec2 texCoord{};
            bool visible = false;
        };

        struct ProjectedTriangle
        {
            ProjectedVertex a{};
            ProjectedVertex b{};
            ProjectedVertex c{};
            GltfVec3 normal{};
            int materialIndex = -1;
            ImU32 color = IM_COL32_WHITE;
            float depth = 0.0f;
            bool doubleSided = false;
            bool hasNormals = false;
        };

        struct ProjectedLine
        {
            ProjectedVertex a{};
            ProjectedVertex b{};
        };

        struct ProjectedPoint
        {
            ProjectedVertex vertex{};
        };

        struct SceneBounds
        {
            GltfVec3 minimum{};
            GltfVec3 maximum{};
        };

        GltfVec3 Add(GltfVec3 left, GltfVec3 right)
        {
            return {left.x + right.x, left.y + right.y, left.z + right.z};
        }

        GltfVec3 Subtract(GltfVec3 left, GltfVec3 right)
        {
            return {left.x - right.x, left.y - right.y, left.z - right.z};
        }

        GltfVec3 Multiply(GltfVec3 value, float scalar)
        {
            return {value.x * scalar, value.y * scalar, value.z * scalar};
        }

        float Dot(GltfVec3 left, GltfVec3 right)
        {
            return glm::dot(glm::vec3(left.x, left.y, left.z),
                            glm::vec3(right.x, right.y, right.z));
        }

        GltfVec3 Cross(GltfVec3 left, GltfVec3 right)
        {
            const glm::vec3 result = glm::cross(glm::vec3(left.x, left.y, left.z),
                                                glm::vec3(right.x, right.y, right.z));
            return {result.x, result.y, result.z};
        }

        GltfVec3 Normalize(GltfVec3 value)
        {
            const glm::vec3 input(value.x, value.y, value.z);
            const float length = glm::length(input);
            if (length <= kEpsilon)
                return {0.0f, 0.0f, 1.0f};
            const glm::vec3 result = glm::normalize(input);
            return {result.x, result.y, result.z};
        }

        GltfVec3 Rotate(GltfVec3 value, float yaw, float pitch)
        {
            const float cy = std::cos(yaw);
            const float sy = std::sin(yaw);
            const float cp = std::cos(pitch);
            const float sp = std::sin(pitch);
            const GltfVec3 yawed{
                value.x * cy - value.z * sy, value.y, value.x * sy + value.z * cy};
            return {yawed.x, yawed.y * cp - yawed.z * sp,
                    yawed.y * sp + yawed.z * cp};
        }

        SceneBounds Bounds(const GltfScene& scene)
        {
            SceneBounds result{scene.boundsMin, scene.boundsMax};
            const GltfVec3 extent = Subtract(result.maximum, result.minimum);
            if (extent.x > kEpsilon || extent.y > kEpsilon || extent.z > kEpsilon)
                return result;

            result.minimum = {std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max()};
            result.maximum = {-std::numeric_limits<float>::max(),
                              -std::numeric_limits<float>::max(),
                              -std::numeric_limits<float>::max()};
            for (const GltfMesh& mesh : scene.meshes)
                for (const GltfVertex& vertex : mesh.vertices)
                {
                    result.minimum.x = (std::min)(result.minimum.x, vertex.position.x);
                    result.minimum.y = (std::min)(result.minimum.y, vertex.position.y);
                    result.minimum.z = (std::min)(result.minimum.z, vertex.position.z);
                    result.maximum.x = (std::max)(result.maximum.x, vertex.position.x);
                    result.maximum.y = (std::max)(result.maximum.y, vertex.position.y);
                    result.maximum.z = (std::max)(result.maximum.z, vertex.position.z);
                }
            return result;
        }

        bool ProjectPoint(GltfVec3 value, const GltfVec3& center,
                          const ModelViewerState& state, ImVec2 canvasSize, float radius,
                          ProjectedVertex& output)
        {
            output.viewPosition = Rotate(Subtract(value, center), state.yaw, state.pitch);
            output.depth = output.viewPosition.z + state.distance;
            output.visible = output.depth > 0.05f;
            if (!output.visible)
                return false;

            const float projection = state.distance / (std::max)(0.05f, output.depth);
            output.position = {
                canvasSize.x * 0.5f + state.pan.x +
                    output.viewPosition.x * projection * canvasSize.y / radius,
                canvasSize.y * 0.5f + state.pan.y -
                    output.viewPosition.y * projection * canvasSize.y / radius};
            return true;
        }

        ImVec4 Color(ImU32 value)
        {
            return ImGui::ColorConvertU32ToFloat4(value);
        }

        ImU32 ColorFromVector(float red, float green, float blue, float alpha = 1.0f)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(
                ImClamp(red, 0.0f, 1.0f), ImClamp(green, 0.0f, 1.0f),
                ImClamp(blue, 0.0f, 1.0f), ImClamp(alpha, 0.0f, 1.0f)));
        }

        float WrapCoordinate(float value, int wrapMode)
        {
            if (wrapMode == 33071)
                return ImClamp(value, 0.0f, 1.0f);
            if (wrapMode == 33648)
            {
                const float repeated = value - std::floor(value);
                const int period = static_cast<int>(std::floor(value));
                return (period & 1) != 0 ? 1.0f - repeated : repeated;
            }
            const float repeated = value - std::floor(value);
            return repeated < 0.0f ? repeated + 1.0f : repeated;
        }

        ImVec4 SampleImage(const GltfScene& scene, const GltfTextureInfo& textureInfo,
                           GltfVec2 uv)
        {
            if (textureInfo.textureIndex < 0 ||
                static_cast<std::size_t>(textureInfo.textureIndex) >= scene.textures.size())
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            const GltfTexture& texture = scene.textures[textureInfo.textureIndex];
            if (texture.imageIndex < 0 ||
                static_cast<std::size_t>(texture.imageIndex) >= scene.images.size())
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            const GltfImage& image = scene.images[texture.imageIndex];
            if (!image.Decoded())
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

            int wrapS = 10497;
            int wrapT = 10497;
            if (texture.samplerIndex >= 0 &&
                static_cast<std::size_t>(texture.samplerIndex) < scene.samplers.size())
            {
                const GltfSampler& sampler = scene.samplers[texture.samplerIndex];
                wrapS = sampler.wrapS;
                wrapT = sampler.wrapT;
            }
            const float u = WrapCoordinate(uv.x, wrapS);
            const float v = 1.0f - WrapCoordinate(uv.y, wrapT);
            const int x = ImClamp(static_cast<int>(u * static_cast<float>(image.width)),
                                  0, image.width - 1);
            const int y = ImClamp(static_cast<int>(v * static_cast<float>(image.height)),
                                  0, image.height - 1);
            const std::size_t offset = static_cast<std::size_t>(y) *
                                           static_cast<std::size_t>(image.width) * 4u +
                                       static_cast<std::size_t>(x) * 4u;
            return ImVec4(image.pixels[offset + 0] / 255.0f,
                          image.pixels[offset + 1] / 255.0f,
                          image.pixels[offset + 2] / 255.0f,
                          image.pixels[offset + 3] / 255.0f);
        }

        const GltfMaterial* MaterialAt(const GltfScene& scene, int materialIndex)
        {
            if (materialIndex < 0 ||
                static_cast<std::size_t>(materialIndex) >= scene.materials.size())
                return nullptr;
            return &scene.materials[materialIndex];
        }

        ImU32 MaterialColor(const GltfScene& scene, int materialIndex, GltfVec2 uv,
                            ImU32 fallback)
        {
            const GltfMaterial* material = MaterialAt(scene, materialIndex);
            if (!material)
                return fallback;
            ImVec4 value(material->baseColorFactor.x, material->baseColorFactor.y,
                         material->baseColorFactor.z, material->baseColorFactor.w);
            const ImVec4 texture = SampleImage(scene, material->baseColorTexture, uv);
            value.x *= texture.x;
            value.y *= texture.y;
            value.z *= texture.z;
            value.w *= texture.w;
            return ImGui::ColorConvertFloat4ToU32(value);
        }

        ImU32 Shade(ImU32 baseColor, GltfVec3 normal, const ModelViewerState& state)
        {
            const GltfVec3 light = Normalize({-0.35f, 0.75f, 0.6f});
            const float diffuse = (std::max)(0.0f, Dot(Normalize(normal), light));
            const float environment = ImClamp(state.environmentLight, 0.0f, 2.0f);
            const float intensity = ImClamp(0.18f + environment * 0.42f + diffuse * 0.72f,
                                            0.05f, 1.5f);
            ImVec4 value = Color(baseColor);
            value.x *= intensity * (0.75f + state.environmentColor.x * 0.25f);
            value.y *= intensity * (0.75f + state.environmentColor.y * 0.25f);
            value.z *= intensity * (0.75f + state.environmentColor.z * 0.25f);
            return ImGui::ColorConvertFloat4ToU32(value);
        }

        bool IsDoubleSided(const GltfScene& scene, int materialIndex)
        {
            const GltfMaterial* material = MaterialAt(scene, materialIndex);
            return material && material->doubleSided;
        }

        void AddTriangle(const GltfScene& scene, const ModelViewerState& state, ImU32 fallback,
                         const std::vector<ProjectedVertex>& vertices, std::uint32_t ia,
                         std::uint32_t ib, std::uint32_t ic, int materialIndex,
                         bool hasNormals,
                         std::vector<ProjectedTriangle>& output)
        {
            if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
                return;
            const ProjectedVertex& a = vertices[ia];
            const ProjectedVertex& b = vertices[ib];
            const ProjectedVertex& c = vertices[ic];
            if (!a.visible || !b.visible || !c.visible)
                return;
            const GltfVec3 faceNormal = Normalize(Cross(
                Subtract(b.viewPosition, a.viewPosition),
                Subtract(c.viewPosition, a.viewPosition)));
            const GltfVec2 uv{
                (a.texCoord.x + b.texCoord.x + c.texCoord.x) / 3.0f,
                (a.texCoord.y + b.texCoord.y + c.texCoord.y) / 3.0f};
            const ImU32 baseColor = MaterialColor(scene, materialIndex, uv, fallback);
            output.push_back({a, b, c, faceNormal, materialIndex,
                              Shade(baseColor, faceNormal, state),
                              (a.depth + b.depth + c.depth) / 3.0f,
                              IsDoubleSided(scene, materialIndex), hasNormals});
        }

        void BuildGeometry(const GltfScene& scene, const ModelViewerState& state,
                           bool isolateSelectedMesh,
                           ImU32 fallback, const GltfVec3& center, float radius,
                           ImVec2 canvasSize, std::vector<ProjectedTriangle>& triangles,
                           std::vector<ProjectedLine>& lines,
                           std::vector<ProjectedPoint>& points)
        {
            for (std::size_t meshIndex = 0; meshIndex < scene.meshes.size(); ++meshIndex)
            {
                if (isolateSelectedMesh && state.selectedMesh >= 0 &&
                    state.selectedMesh != static_cast<int>(meshIndex))
                    continue;
                const GltfMesh& mesh = scene.meshes[meshIndex];
                std::vector<ProjectedVertex> vertices(mesh.vertices.size());
                for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex)
                {
                    ProjectedVertex& projected = vertices[vertexIndex];
                    ProjectPoint(mesh.vertices[vertexIndex].position, center, state,
                                 canvasSize, radius, projected);
                    projected.viewNormal = Rotate(
                        mesh.hasNormals ? mesh.vertices[vertexIndex].normal :
                                          GltfVec3{0.0f, 0.0f, 1.0f},
                        state.yaw, state.pitch);
                    projected.texCoord = mesh.vertices[vertexIndex].texCoord;
                    if (projected.visible)
                        points.push_back({projected});
                }

                const auto addLine = [&](std::uint32_t ia, std::uint32_t ib) {
                    if (ia < vertices.size() && ib < vertices.size() &&
                        vertices[ia].visible && vertices[ib].visible)
                        lines.push_back({vertices[ia], vertices[ib]});
                };
                const auto addTriangles = [&](std::uint32_t ia, std::uint32_t ib,
                                              std::uint32_t ic) {
                    AddTriangle(scene, state, fallback, vertices, ia, ib, ic,
                                mesh.materialIndex, mesh.hasNormals, triangles);
                };

                switch (mesh.primitiveType)
                {
                case GltfPrimitiveType::Points:
                    break;
                case GltfPrimitiveType::Lines:
                    for (std::size_t index = 0; index + 1 < mesh.indices.size(); index += 2)
                        addLine(mesh.indices[index], mesh.indices[index + 1]);
                    break;
                case GltfPrimitiveType::LineLoop:
                    for (std::size_t index = 0; index < mesh.indices.size(); ++index)
                        addLine(mesh.indices[index], mesh.indices[(index + 1) % mesh.indices.size()]);
                    break;
                case GltfPrimitiveType::LineStrip:
                    for (std::size_t index = 0; index + 1 < mesh.indices.size(); ++index)
                        addLine(mesh.indices[index], mesh.indices[index + 1]);
                    break;
                case GltfPrimitiveType::TriangleStrip:
                    for (std::size_t index = 0; index + 2 < mesh.indices.size(); ++index)
                    {
                        const std::uint32_t a = mesh.indices[index];
                        const std::uint32_t b = mesh.indices[index + 1];
                        const std::uint32_t c = mesh.indices[index + 2];
                        if ((index & 1u) != 0)
                            addTriangles(b, a, c);
                        else
                            addTriangles(a, b, c);
                    }
                    break;
                case GltfPrimitiveType::TriangleFan:
                    for (std::size_t index = 1; index + 1 < mesh.indices.size(); ++index)
                        addTriangles(mesh.indices[0], mesh.indices[index], mesh.indices[index + 1]);
                    break;
                case GltfPrimitiveType::Triangles:
                default:
                    for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
                        addTriangles(mesh.indices[index], mesh.indices[index + 1],
                                     mesh.indices[index + 2]);
                    break;
                }
            }
        }

        float SignedArea(const ProjectedTriangle& triangle)
        {
            return (triangle.b.position.x - triangle.a.position.x) *
                       (triangle.c.position.y - triangle.a.position.y) -
                   (triangle.b.position.y - triangle.a.position.y) *
                       (triangle.c.position.x - triangle.a.position.x);
        }

        void DrawBackground(ImDrawList& draw, ImVec2 minimum, ImVec2 size,
                            const ModelViewerOptions& options,
                            ModelViewerBackground background)
        {
            const ImVec2 maximum = minimum + size;
            if (background == ModelViewerBackground::Solid)
            {
                draw.AddRectFilled(minimum, maximum, options.background);
                return;
            }
            if (background == ModelViewerBackground::Gradient)
            {
                draw.AddRectFilledMultiColor(minimum, maximum, options.background,
                                              options.background, options.backgroundSecondary,
                                              options.backgroundSecondary);
                return;
            }

            draw.AddRectFilled(minimum, maximum, options.background);
            const float cell = 18.0f * GetTheme().Scale;
            for (int row = 0; row * cell < size.y + cell; ++row)
                for (int column = 0; column * cell < size.x + cell; ++column)
                    if (((row + column) & 1) != 0)
                    {
                        const ImVec2 cellMinimum(minimum.x + column * cell,
                                                  minimum.y + row * cell);
                        const ImVec2 cellMaximum(
                            (std::min)(cellMinimum.x + cell, maximum.x),
                            (std::min)(cellMinimum.y + cell, maximum.y));
                        draw.AddRectFilled(cellMinimum, cellMaximum,
                                           options.backgroundSecondary);
                    }
        }

        bool ProjectGridPoint(GltfVec3 point, const GltfVec3& center,
                              const ModelViewerState& state, ImVec2 canvasSize, float radius,
                              ImVec2& output)
        {
            ProjectedVertex projected;
            if (!ProjectPoint(point, center, state, canvasSize, radius, projected))
                return false;
            output = projected.position;
            return true;
        }

        void DrawGrid(ImDrawList& draw, ImVec2 cursor, ImVec2 size, const GltfVec3& center,
                      float radius, const ModelViewerState& state, ImU32 color)
        {
            const float extent = radius * 2.5f;
            const float step = extent / 8.0f;
            const float floor = center.y - radius * 0.95f;
            for (int index = -8; index <= 8; ++index)
            {
                ImVec2 first;
                ImVec2 last;
                if (ProjectGridPoint({center.x + index * step, floor, center.z - extent},
                                     center, state, size, radius, first) &&
                    ProjectGridPoint({center.x + index * step, floor, center.z + extent},
                                     center, state, size, radius, last))
                    draw.AddLine(cursor + first, cursor + last, color, 1.0f);
                if (ProjectGridPoint({center.x - extent, floor, center.z + index * step},
                                     center, state, size, radius, first) &&
                    ProjectGridPoint({center.x + extent, floor, center.z + index * step},
                                     center, state, size, radius, last))
                    draw.AddLine(cursor + first, cursor + last, color, 1.0f);
            }
        }

        void DrawGeometry(ImDrawList& draw, ImVec2 cursor, const ModelViewerOptions& options,
                          const ModelViewerState& state,
                          std::vector<ProjectedTriangle>& triangles,
                          const std::vector<ProjectedLine>& lines,
                          const std::vector<ProjectedPoint>& points)
        {
            const bool wireframe = state.wireframe || options.mode == ModelViewMode::Wireframe;
            if (options.mode == ModelViewMode::Points)
            {
                for (const ProjectedPoint& point : points)
                    draw.AddCircleFilled(cursor + point.vertex.position, 2.0f,
                                         options.foreground);
                return;
            }

            for (const ProjectedLine& line : lines)
                draw.AddLine(cursor + line.a.position, cursor + line.b.position,
                             options.wireframe, 1.0f);

            std::sort(triangles.begin(), triangles.end(),
                      [](const ProjectedTriangle& left, const ProjectedTriangle& right) {
                          return left.depth > right.depth;
                      });
            for (const ProjectedTriangle& triangle : triangles)
            {
                if (!wireframe && !triangle.doubleSided && SignedArea(triangle) <= 0.0f)
                    continue;
                const ImVec2 vertices[3] = {
                    cursor + triangle.a.position, cursor + triangle.b.position,
                    cursor + triangle.c.position};
                if (wireframe)
                    draw.AddPolyline(vertices, 3, options.wireframe, 1.0f,
                                     ImDrawFlags_Closed);
                else
                {
                    draw.AddTriangleFilled(vertices[0], vertices[1], vertices[2],
                                           triangle.color);
                    draw.AddTriangle(vertices[0], vertices[1], vertices[2],
                                     WithAlphaMul(options.wireframe, 0.45f), 1.0f);
                }
            }
        }

        std::uint8_t ToByte(float value)
        {
            return static_cast<std::uint8_t>(ImClamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        void SetPixel(ModelViewerOffscreenImage& image, int x, int y, ImVec4 color)
        {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height)
                return;
            const std::size_t offset = static_cast<std::size_t>(y) *
                                           static_cast<std::size_t>(image.strideBytes) +
                                       static_cast<std::size_t>(x) * 4u;
            image.pixels[offset + 0] = ToByte(color.x);
            image.pixels[offset + 1] = ToByte(color.y);
            image.pixels[offset + 2] = ToByte(color.z);
            image.pixels[offset + 3] = ToByte(color.w);
        }

        void BlendPixel(ModelViewerOffscreenImage& image, int x, int y, ImVec4 color)
        {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height)
                return;
            const std::size_t offset = static_cast<std::size_t>(y) *
                                           static_cast<std::size_t>(image.strideBytes) +
                                       static_cast<std::size_t>(x) * 4u;
            const float alpha = ImClamp(color.w, 0.0f, 1.0f);
            const float destinationAlpha = image.pixels[offset + 3] / 255.0f;
            const float resultAlpha = alpha + destinationAlpha * (1.0f - alpha);
            if (resultAlpha <= kEpsilon)
                return;
            const float inverse = destinationAlpha * (1.0f - alpha);
            image.pixels[offset + 0] = ToByte((color.x * alpha +
                                               image.pixels[offset + 0] / 255.0f * inverse) /
                                              resultAlpha);
            image.pixels[offset + 1] = ToByte((color.y * alpha +
                                               image.pixels[offset + 1] / 255.0f * inverse) /
                                              resultAlpha);
            image.pixels[offset + 2] = ToByte((color.z * alpha +
                                               image.pixels[offset + 2] / 255.0f * inverse) /
                                              resultAlpha);
            image.pixels[offset + 3] = ToByte(resultAlpha);
        }

        void RasterLine(ModelViewerOffscreenImage& image, ImVec2 start, ImVec2 end,
                        ImVec4 color, float scaleX, float scaleY)
        {
            start.x *= scaleX;
            start.y *= scaleY;
            end.x *= scaleX;
            end.y *= scaleY;
            const int x0 = static_cast<int>(std::floor(start.x));
            const int y0 = static_cast<int>(std::floor(start.y));
            const int x1 = static_cast<int>(std::floor(end.x));
            const int y1 = static_cast<int>(std::floor(end.y));
            const int dx = std::abs(x1 - x0);
            const int sx = x0 < x1 ? 1 : -1;
            const int dy = -std::abs(y1 - y0);
            const int sy = y0 < y1 ? 1 : -1;
            int x = x0;
            int y = y0;
            int error = dx + dy;
            for (;;)
            {
                BlendPixel(image, x, y, color);
                if (x == x1 && y == y1)
                    break;
                const int doubleError = 2 * error;
                if (doubleError >= dy)
                {
                    error += dy;
                    x += sx;
                }
                if (doubleError <= dx)
                {
                    error += dx;
                    y += sy;
                }
            }
        }

        float Edge(ImVec2 a, ImVec2 b, ImVec2 point)
        {
            return (point.x - a.x) * (b.y - a.y) -
                   (point.y - a.y) * (b.x - a.x);
        }

        void RasterTriangle(ModelViewerOffscreenImage& image,
                            std::vector<float>& depthBuffer,
                            const GltfScene& scene, const ModelViewerState& state,
                            const ModelViewerOptions& options,
                            const ProjectedTriangle& triangle, float scaleX, float scaleY)
        {
            if (!triangle.doubleSided && SignedArea(triangle) <= 0.0f)
                return;
            const ImVec2 a{triangle.a.position.x * scaleX, triangle.a.position.y * scaleY};
            const ImVec2 b{triangle.b.position.x * scaleX, triangle.b.position.y * scaleY};
            const ImVec2 c{triangle.c.position.x * scaleX, triangle.c.position.y * scaleY};
            const float area = Edge(a, b, c);
            if (std::fabs(area) <= kEpsilon)
                return;
            const int minimumX = ImClamp(static_cast<int>(std::floor(
                (std::min)(a.x, (std::min)(b.x, c.x)))), 0, image.width - 1);
            const int maximumX = ImClamp(static_cast<int>(std::ceil(
                (std::max)(a.x, (std::max)(b.x, c.x)))), 0, image.width - 1);
            const int minimumY = ImClamp(static_cast<int>(std::floor(
                (std::min)(a.y, (std::min)(b.y, c.y)))), 0, image.height - 1);
            const int maximumY = ImClamp(static_cast<int>(std::ceil(
                (std::max)(a.y, (std::max)(b.y, c.y)))), 0, image.height - 1);
            for (int y = minimumY; y <= maximumY; ++y)
                for (int x = minimumX; x <= maximumX; ++x)
                {
                    const ImVec2 point(x + 0.5f, y + 0.5f);
                    const float weightA = Edge(b, c, point) / area;
                    const float weightB = Edge(c, a, point) / area;
                    const float weightC = Edge(a, b, point) / area;
                    if (weightA < 0.0f || weightB < 0.0f || weightC < 0.0f)
                        continue;
                    const float depth = weightA * triangle.a.depth +
                                        weightB * triangle.b.depth + weightC * triangle.c.depth;
                    const std::size_t depthIndex = static_cast<std::size_t>(y) *
                                                       static_cast<std::size_t>(image.width) +
                                                   static_cast<std::size_t>(x);
                    if (depth >= depthBuffer[depthIndex])
                        continue;
                    depthBuffer[depthIndex] = depth;
                    const GltfVec2 uv{
                        weightA * triangle.a.texCoord.x + weightB * triangle.b.texCoord.x +
                            weightC * triangle.c.texCoord.x,
                        weightA * triangle.a.texCoord.y + weightB * triangle.b.texCoord.y +
                            weightC * triangle.c.texCoord.y};
                    const GltfVec3 normal = triangle.hasNormals ? Normalize(Add(
                        Add(Multiply(triangle.a.viewNormal, weightA),
                            Multiply(triangle.b.viewNormal, weightB)),
                        Multiply(triangle.c.viewNormal, weightC))) : triangle.normal;
                    const ImU32 base = MaterialColor(scene, triangle.materialIndex, uv,
                                                     options.foreground);
                    BlendPixel(image, x, y, Color(Shade(base, normal, state)));
                }
        }

        void RasterDisc(ModelViewerOffscreenImage& image, ImVec2 center, float radius,
                         ImVec4 color, float scaleX, float scaleY)
        {
            center.x *= scaleX;
            center.y *= scaleY;
            radius *= (scaleX + scaleY) * 0.5f;
            const int minimumX = static_cast<int>(std::floor(center.x - radius));
            const int maximumX = static_cast<int>(std::ceil(center.x + radius));
            const int minimumY = static_cast<int>(std::floor(center.y - radius));
            const int maximumY = static_cast<int>(std::ceil(center.y + radius));
            for (int y = minimumY; y <= maximumY; ++y)
                for (int x = minimumX; x <= maximumX; ++x)
                {
                    const float dx = x + 0.5f - center.x;
                    const float dy = y + 0.5f - center.y;
                    if (dx * dx + dy * dy <= radius * radius)
                        BlendPixel(image, x, y, color);
                }
        }

        void RenderOffscreen(ModelViewerFrame& frame, const GltfScene& scene,
                             const ModelViewerState& state, const ModelViewerOptions& options,
                             ImVec2 canvasSize, const GltfVec3& center, float radius,
                             const std::vector<ProjectedTriangle>& triangles,
                             const std::vector<ProjectedLine>& lines,
                             const std::vector<ProjectedPoint>& points)
        {
            const int width = frame.image.width > 0 ? frame.image.width :
                (std::max)(1, static_cast<int>(std::ceil(canvasSize.x)));
            const int height = frame.image.height > 0 ? frame.image.height :
                (std::max)(1, static_cast<int>(std::ceil(canvasSize.y)));
            frame.image.Resize(width, height);
            frame.ResetTexture();
            const float scaleX = width / (std::max)(1.0f, canvasSize.x);
            const float scaleY = height / (std::max)(1.0f, canvasSize.y);

            const ImVec4 background = Color(options.background);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                {
                    ImVec4 value = background;
                    if (state.background == ModelViewerBackground::Checker)
                    {
                        const int cellX = static_cast<int>(x / (18.0f * scaleX));
                        const int cellY = static_cast<int>(y / (18.0f * scaleY));
                        if (((cellX + cellY) & 1) != 0)
                            value = Color(options.backgroundSecondary);
                    }
                    else if (state.background == ModelViewerBackground::Gradient)
                    {
                        const ImVec4 secondary = Color(options.backgroundSecondary);
                        const float factor = height > 1 ? static_cast<float>(y) /
                            static_cast<float>(height - 1) : 0.0f;
                        value.x = background.x + (secondary.x - background.x) * factor;
                        value.y = background.y + (secondary.y - background.y) * factor;
                        value.z = background.z + (secondary.z - background.z) * factor;
                        value.w = background.w + (secondary.w - background.w) * factor;
                    }
                    SetPixel(frame.image, x, y, value);
                }

            if (state.showGrid)
            {
                const ImVec4 gridColor = Color(WithAlphaMul(options.wireframe, 0.32f));
                const float extent = radius * 2.5f;
                const float step = extent / 8.0f;
                const float floor = center.y - radius * 0.95f;
                for (int index = -8; index <= 8; ++index)
                {
                    ImVec2 first;
                    ImVec2 last;
                    if (ProjectGridPoint({center.x + index * step, floor, center.z - extent},
                                         center, state, canvasSize, radius, first) &&
                        ProjectGridPoint({center.x + index * step, floor, center.z + extent},
                                         center, state, canvasSize, radius, last))
                        RasterLine(frame.image, first, last, gridColor, scaleX, scaleY);
                    if (ProjectGridPoint({center.x - extent, floor, center.z + index * step},
                                         center, state, canvasSize, radius, first) &&
                        ProjectGridPoint({center.x + extent, floor, center.z + index * step},
                                         center, state, canvasSize, radius, last))
                        RasterLine(frame.image, first, last, gridColor, scaleX, scaleY);
                }
            }

            std::vector<float> depthBuffer(static_cast<std::size_t>(width) *
                                               static_cast<std::size_t>(height),
                                           std::numeric_limits<float>::max());
            const bool wireframe = state.wireframe || options.mode == ModelViewMode::Wireframe;
            if (options.mode == ModelViewMode::Points)
                for (const ProjectedPoint& point : points)
                    RasterDisc(frame.image, point.vertex.position, 2.0f,
                               Color(options.foreground), scaleX, scaleY);
            else
            {
                for (const ProjectedLine& line : lines)
                    RasterLine(frame.image, line.a.position, line.b.position,
                               Color(options.wireframe), scaleX, scaleY);
                std::vector<ProjectedTriangle> sorted = triangles;
                std::sort(sorted.begin(), sorted.end(),
                          [](const ProjectedTriangle& left, const ProjectedTriangle& right) {
                              return left.depth > right.depth;
                          });
                for (const ProjectedTriangle& triangle : sorted)
                {
                    if (wireframe)
                    {
                        const ImVec4 lineColor = Color(options.wireframe);
                        RasterLine(frame.image, triangle.a.position, triangle.b.position,
                                   lineColor, scaleX, scaleY);
                        RasterLine(frame.image, triangle.b.position, triangle.c.position,
                                   lineColor, scaleX, scaleY);
                        RasterLine(frame.image, triangle.c.position, triangle.a.position,
                                   lineColor, scaleX, scaleY);
                    }
                    else
                        RasterTriangle(frame.image, depthBuffer, scene, state, options,
                                       triangle, scaleX, scaleY);
                }
            }
        }

        void DrawMaterialInspector(const GltfScene& scene, int selectedMesh)
        {
            if (selectedMesh < 0 || static_cast<std::size_t>(selectedMesh) >= scene.meshes.size())
            {
                ImGui::TextDisabled("Select a mesh");
                return;
            }
            const GltfMesh& mesh = scene.meshes[selectedMesh];
            ImGui::Separator();
            ImGui::TextUnformatted("Material");
            const GltfMaterial* material = MaterialAt(scene, mesh.materialIndex);
            if (!material)
            {
                ImGui::TextDisabled("None");
                return;
            }
            ImGui::TextUnformatted(material->name.empty() ? "Unnamed material" : material->name.c_str());
            ImGui::ColorButton("##base-color", ImVec4(material->baseColorFactor.x,
                                                        material->baseColorFactor.y,
                                                        material->baseColorFactor.z,
                                                        material->baseColorFactor.w));
            ImGui::SameLine();
            ImGui::Text("Metal %.2f  Rough %.2f", material->metallicFactor,
                        material->roughnessFactor);
            ImGui::Text("Alpha: %s%s", material->alphaMode == GltfAlphaMode::Blend ? "Blend" :
                                      material->alphaMode == GltfAlphaMode::Mask ? "Mask" : "Opaque",
                        material->doubleSided ? "  Double-sided" : "");
            if (material->baseColorTexture.textureIndex >= 0)
                ImGui::Text("Base texture: %d", material->baseColorTexture.textureIndex);
            if (material->normalTexture.textureIndex >= 0)
                ImGui::Text("Normal texture: %d", material->normalTexture.textureIndex);
        }
    }

    bool ModelViewer(const char* id, const GltfScene& scene, ModelViewerState& state,
                     const ModelViewerOptions& options)
    {
        if (!id || scene.Empty())
            return false;

        const Theme& theme = GetTheme();
        const ImVec2 size(
            options.size.x > 0.0f ? options.size.x : ImGui::GetContentRegionAvail().x,
            options.size.y > 0.0f ? options.size.y : 280.0f * theme.Scale);
        if (state.selectedMesh >= static_cast<int>(scene.meshes.size()))
            state.selectedMesh = -1;

        bool pressed = false;
        ImGui::PushID(id);
        ImGui::BeginGroup();
        const float controlsTop = ImGui::GetCursorScreenPos().y;
        if (options.showControls)
        {
            ImGui::Checkbox("Rotate", &state.autoRotate);
            ImGui::SameLine();
            ImGui::Checkbox("Grid", &state.showGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Wire", &state.wireframe);
            ImGui::SameLine();
            int background = static_cast<int>(state.background);
            const char* backgrounds = "Solid\0Checker\0Gradient\0";
            ImGui::SetNextItemWidth(110.0f * theme.Scale);
            if (ImGui::Combo("Background", &background, backgrounds))
                state.background = static_cast<ModelViewerBackground>(ImClamp(background, 0, 2));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f * theme.Scale);
            ImGui::SliderFloat("Light", &state.environmentLight, 0.0f, 2.0f, "%.2f");
            ImGui::NewLine();
        }

        const float contentHeight = (std::max)(1.0f,
            size.y - (ImGui::GetCursorScreenPos().y - controlsTop));
        const bool showMeshList = options.showMeshList && size.x >= 340.0f * theme.Scale;
        const float listWidth = showMeshList ?
            ImClamp(size.x * 0.25f, 128.0f * theme.Scale, 220.0f * theme.Scale) : 0.0f;
        const ImVec2 viewportSize(
            (std::max)(1.0f, size.x - (showMeshList ? listWidth + ImGui::GetStyle().ItemSpacing.x : 0.0f)),
            contentHeight);

        if (showMeshList)
        {
            if (ImGui::BeginChild("##mesh-list", ImVec2(listWidth, contentHeight),
                                  ImGuiChildFlags_Borders))
            {
                ImGui::TextUnformatted("Meshes");
                for (std::size_t meshIndex = 0; meshIndex < scene.meshes.size(); ++meshIndex)
                {
                    const GltfMesh& mesh = scene.meshes[meshIndex];
                    char fallback[32] = {};
                    if (mesh.name.empty())
                        std::snprintf(fallback, sizeof(fallback), "Mesh %zu", meshIndex);
                    const char* label = mesh.name.empty() ? fallback : mesh.name.c_str();
                    ImGui::PushID(static_cast<int>(meshIndex));
                    if (ImGui::Selectable(label,
                                         state.selectedMesh == static_cast<int>(meshIndex)))
                        state.selectedMesh = static_cast<int>(meshIndex);
                    ImGui::PopID();
                }
                if (options.showMaterialInspector)
                    DrawMaterialInspector(scene, state.selectedMesh);
            }
            ImGui::EndChild();
            ImGui::SameLine();
        }

        if (ImGui::BeginChild("##viewport", viewportSize, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const ImVec2 canvasCursor = ImGui::GetCursorScreenPos();
            const bool canvasPressed = ImGui::InvisibleButton("##canvas", viewportSize);
            pressed = canvasPressed;
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                state.yaw += delta.x * 0.01f;
                state.pitch = ImClamp(state.pitch + delta.y * 0.01f, -1.5f, 1.5f);
            }
            if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                state.pan += ImGui::GetIO().MouseDelta;

            const SceneBounds bounds = Bounds(scene);
            const GltfVec3 center{
                (bounds.minimum.x + bounds.maximum.x) * 0.5f,
                (bounds.minimum.y + bounds.maximum.y) * 0.5f,
                (bounds.minimum.z + bounds.maximum.z) * 0.5f};
            const GltfVec3 extent = Subtract(bounds.maximum, bounds.minimum);
            const float radius = (std::max)(0.001f,
                (std::max)(extent.x, (std::max)(extent.y, extent.z)) * 0.5f);
            if (state.distance <= 0.0f)
                state.distance = radius * 3.0f;
            if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
                state.distance = ImClamp(state.distance *
                    std::pow(0.86f, ImGui::GetIO().MouseWheel), radius * 0.15f, radius * 100.0f);
            if (state.autoRotate)
                state.yaw += ImGui::GetIO().DeltaTime * 0.45f;

            std::vector<ProjectedTriangle> triangles;
            std::vector<ProjectedLine> lines;
            std::vector<ProjectedPoint> points;
            BuildGeometry(scene, state, options.isolateSelectedMesh, options.foreground,
                          center, radius, viewportSize, triangles, lines, points);

            ElementOptions elementOptions;
            elementOptions.role = ElementRole::ModelViewer;
            if (options.element)
                elementOptions = *options.element;
            ElementScope element = BeginElement(ImGui::GetWindowDrawList(), ImGui::GetItemID(),
                                                true, elementOptions);
            if (element)
            {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                DrawBackground(*draw, canvasCursor, viewportSize, options, state.background);
                if (state.showGrid)
                    DrawGrid(*draw, canvasCursor, viewportSize, center, radius, state,
                             WithAlphaMul(options.wireframe, 0.32f));

                if (options.frame)
                {
                    RenderOffscreen(*options.frame, scene, state, options, viewportSize,
                                    center, radius, triangles, lines, points);
                    if (options.textureBridge && options.frame->image.Valid())
                        options.frame->texture = options.textureBridge(
                            options.frame->image, options.textureUserData);
                    options.frame->textureDisplayed =
                        options.frame->texture != ImTextureID_Invalid;
                    if (options.frame->textureDisplayed)
                        draw->AddImage(ImTextureRef(options.frame->texture), canvasCursor,
                                       canvasCursor + viewportSize);
                    else
                        DrawGeometry(*draw, canvasCursor, options, state, triangles, lines, points);
                }
                else
                    DrawGeometry(*draw, canvasCursor, options, state, triangles, lines, points);

                draw->AddRect(canvasCursor, canvasCursor + viewportSize, GetTheme().Border,
                              GetTheme().RoundingControl * theme.Scale, 0,
                              GetTheme().BorderThickness * theme.Scale);
            }
            element.End();
        }
        ImGui::EndChild();
        ImGui::EndGroup();
        ImGui::PopID();
        return pressed;
    }
}
