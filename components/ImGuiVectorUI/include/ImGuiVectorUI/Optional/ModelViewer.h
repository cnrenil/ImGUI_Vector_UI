/** @file ModelViewer.h @brief Backend-neutral CPU glTF model viewer. */
#pragma once

#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Optional/Gltf.h"

#include <cstdint>
#include <vector>

namespace IVUI
{
    enum class ModelViewMode { Solid, Wireframe, Points };
    enum class ModelViewerBackground { Solid, Checker, Gradient };

    struct ModelViewerState
    {
        float yaw = 0.65f;
        float pitch = 0.35f;
        float distance = 0.0f; ///< Zero fits to the current scene on first draw.
        ImVec2 pan{};
        bool autoRotate = false;
        bool wireframe = false;
        bool showGrid = false;
        int selectedMesh = -1; ///< Negative selects all meshes.
        float environmentLight = 0.65f;
        ImVec4 environmentColor{0.78f, 0.84f, 1.0f, 1.0f};
        ModelViewerBackground background = ModelViewerBackground::Solid;
    };

    /** CPU RGBA8 image handed to an optional application texture uploader. */
    struct ModelViewerOffscreenImage
    {
        int width = 0;
        int height = 0;
        int strideBytes = 0;
        std::vector<std::uint8_t> pixels;

        void Resize(int newWidth, int newHeight)
        {
            width = newWidth > 0 ? newWidth : 0;
            height = newHeight > 0 ? newHeight : 0;
            strideBytes = width * 4;
            pixels.assign(static_cast<std::size_t>(strideBytes) *
                              static_cast<std::size_t>(height), 0u);
        }

        bool Valid() const noexcept
        {
            return width > 0 && height > 0 && strideBytes >= width * 4 &&
                pixels.size() >= static_cast<std::size_t>(strideBytes) *
                    static_cast<std::size_t>(height);
        }
    };

    /** Per-frame CPU image plus the optional renderer-owned ImGui texture identifier. */
    struct ModelViewerFrame
    {
        ModelViewerOffscreenImage image;
        ImTextureID texture = ImTextureID_Invalid;
        bool textureDisplayed = false;

        void ResetTexture() noexcept
        {
            texture = ImTextureID_Invalid;
            textureDisplayed = false;
        }
    };

    /** Returns an ImGui texture identifier for the supplied CPU RGBA8 image. */
    using ModelViewerTextureBridge = ImTextureID (*)(
        const ModelViewerOffscreenImage& image, void* userData);

    struct ModelViewerOptions
    {
        ImVec2 size{0.0f, 280.0f};
        ModelViewMode mode = ModelViewMode::Solid;
        ImU32 background = IM_COL32(12, 14, 18, 220);
        ImU32 foreground = IM_COL32(190, 210, 235, 255);
        ImU32 wireframe = IM_COL32(100, 170, 230, 220);
        const ElementOptions* element = nullptr;
        bool showControls = true;
        bool showMeshList = true;
        bool showMaterialInspector = true;
        bool isolateSelectedMesh = false;
        ImU32 backgroundSecondary = IM_COL32(22, 26, 34, 220);
        ModelViewerFrame* frame = nullptr;
        ModelViewerTextureBridge textureBridge = nullptr;
        void* textureUserData = nullptr;
    };

    /**
     * Renders a CPU glTF scene with orbit, pan, zoom, mesh selection, material inspection,
     * environment lighting, wireframe/grid modes and selectable backgrounds. When a frame
     * and texture bridge are supplied, the CPU image is offered to the bridge and its valid
     * texture is displayed; otherwise geometry is drawn directly through ImDrawList.
     */
    bool ModelViewer(const char* id, const GltfScene& scene, ModelViewerState& state,
                     const ModelViewerOptions& options = {});
}
