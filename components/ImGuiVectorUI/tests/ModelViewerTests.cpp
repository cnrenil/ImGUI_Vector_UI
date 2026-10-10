#include "ImGuiVectorUI/Optional/ModelViewer.h"
#include "ImGuiVectorUI/UI.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

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

    class TestPlatform final : public IVUI::IPlatformBackend
    {
    public:
        bool Initialize(ImGuiContext&, std::string&) noexcept override { return true; }
        void Shutdown() noexcept override {}
        void NewFrame() noexcept override
        {
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(320.0f, 240.0f);
            io.DeltaTime = 1.0f / 60.0f;
        }
        bool ProcessEvent(const IVUI::PlatformEvent&) noexcept override { return true; }
    };

    class TestRenderer final : public IVUI::IRendererBackend
    {
    public:
        bool Initialize(ImGuiContext&, std::string&) noexcept override { return true; }
        void Shutdown() noexcept override {}
        void NewFrame() noexcept override
        {
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }
        bool RenderDrawData(ImDrawData&, const IVUI::RenderFrame&, IVUI::IEffectRenderer*,
                            std::string&) noexcept override
        {
            return true;
        }
    };

    int g_bridgeCalls = 0;

    ImTextureID Upload(const IVUI::ModelViewerOffscreenImage& image, void*)
    {
        ++g_bridgeCalls;
        Check(image.Valid(), "bridge receives a valid image");
        Check(image.pixels.size() == static_cast<std::size_t>(image.strideBytes) *
                                      static_cast<std::size_t>(image.height),
              "bridge receives complete pixels");
        return static_cast<ImTextureID>(42);
    }
}

int main()
{
    IVUI::UIConfig config;
    config.platform = std::make_unique<TestPlatform>();
    config.renderer = std::make_unique<TestRenderer>();
    std::string error;
    std::unique_ptr<IVUI::UI> ui = IVUI::UI::Create(std::move(config), &error);
    Check(ui != nullptr && error.empty(), "create test UI");
    Check(ui->BeginFrame(), "begin test frame");

    IVUI::GltfScene scene;
    scene.boundsMin = {-1.0f, -1.0f, 0.0f};
    scene.boundsMax = {1.0f, 1.0f, 0.0f};
    IVUI::GltfMesh mesh;
    mesh.name = "Triangle";
    mesh.vertices = {{{-1.0f, -1.0f, 0.0f}},
                     {{1.0f, -1.0f, 0.0f}},
                     {{0.0f, 1.0f, 0.0f}}};
    mesh.indices = {0, 1, 2};
    scene.meshes.push_back(mesh);

    ImGui::SetNextWindowSize(ImVec2(240.0f, 210.0f), ImGuiCond_Always);
    ImGui::Begin("Model viewer test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    IVUI::ModelViewerFrame frame;
    IVUI::ModelViewerOptions options;
    options.size = ImVec2(200.0f, 160.0f);
    options.showControls = false;
    options.showMeshList = false;
    options.frame = &frame;
    options.textureBridge = Upload;
    IVUI::ModelViewerState state;
    Check(!IVUI::ModelViewer("viewer", scene, state, options),
          "model viewer reports no unclicked canvas press");
    ImGui::End();
    ui->EndFrameWithoutRender();

    Check(frame.image.Valid(), "model viewer produces an offscreen image");
    Check(g_bridgeCalls == 1, "model viewer invokes the texture bridge");
    Check(frame.texture == static_cast<ImTextureID>(42) && frame.textureDisplayed,
          "model viewer displays the bridged texture");
    return EXIT_SUCCESS;
}
