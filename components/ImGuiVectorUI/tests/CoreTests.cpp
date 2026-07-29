#include "ImGuiVectorUI/UI.h"
#include "ImGuiVectorUI/Drawing/Effects.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

namespace
{
    void Check(bool condition)
    {
        if (!condition)
            std::exit(EXIT_FAILURE);
    }

    struct Counters
    {
        int platformInit = 0;
        int platformFrames = 0;
        int platformShutdown = 0;
        int rendererInit = 0;
        int rendererFrames = 0;
        int rendererCalls = 0;
        int rendererShutdown = 0;
    };

    class TestPlatform final : public IVUI::IPlatformBackend
    {
    public:
        explicit TestPlatform(Counters& counters) : counters_(counters) {}
        bool Initialize(ImGuiContext&, std::string&) noexcept override
        {
            ++counters_.platformInit;
            return true;
        }
        void Shutdown() noexcept override { ++counters_.platformShutdown; }
        void NewFrame() noexcept override
        {
            ++counters_.platformFrames;
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(800.0f, 600.0f);
            io.DeltaTime = 1.0f / 60.0f;
        }
        bool ProcessEvent(const IVUI::PlatformEvent&) noexcept override { return true; }
    private:
        Counters& counters_;
    };

    struct TestFrame final : IVUI::RenderFrame {};

    class TestRenderer final : public IVUI::IRendererBackend
    {
    public:
        explicit TestRenderer(Counters& counters) : counters_(counters) {}
        bool Initialize(ImGuiContext&, std::string&) noexcept override
        {
            ++counters_.rendererInit;
            return true;
        }
        void Shutdown() noexcept override { ++counters_.rendererShutdown; }
        void NewFrame() noexcept override
        {
            ++counters_.rendererFrames;
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }
        bool RenderDrawData(ImDrawData& data, const IVUI::RenderFrame& frame,
                            IVUI::IEffectRenderer*, std::string&) noexcept override
        {
            Check(dynamic_cast<const TestFrame*>(&frame) != nullptr);
            Check(data.Valid);
            ++counters_.rendererCalls;
            return true;
        }
    private:
        Counters& counters_;
    };

    class TestEffects final : public IVUI::IEffectRenderer
    {
    public:
        IVUI::EffectCapabilities GetCapabilities() const noexcept override
        {
            return IVUI::AllEffectCapabilities;
        }
        void RenderEffect(const IVUI::EffectRequest& request) noexcept override
        {
            last = request.id;
            ++calls;
        }
        IVUI::EffectId last = IVUI::EffectId::BackdropBlur;
        int calls = 0;
    };

    class TestLayout final : public IVUI::ILayout
    {
    public:
        explicit TestLayout(int& calls) : calls_(calls) {}
        void Render(IVUI::UI&, const std::function<void()>& body) override
        {
            ++calls_;
            if (body) body();
        }
    private:
        int& calls_;
    };

    void CustomEffect(void* userData, const IVUI::EffectRequest& request,
                      IVUI::IEffectRenderer& backend) noexcept
    {
        ++*static_cast<int*>(userData);
        backend.RenderEffect(request);
    }

    std::unique_ptr<IVUI::UI> MakeUi(Counters& counters, IVUI::Theme theme)
    {
        IVUI::UIConfig config;
        config.platform = std::make_unique<TestPlatform>(counters);
        config.renderer = std::make_unique<TestRenderer>(counters);
        config.initialTheme = theme;
        std::string error;
        auto ui = IVUI::UI::Create(std::move(config), &error);
        Check(ui != nullptr && error.empty());
        return ui;
    }
}

int main()
{
    Check(IVUI::EaseValue(2.0f, 6.0f, -1.0f, IVUI::EASE_LINEAR) == 2.0f);
    Check(IVUI::EaseValue(2.0f, 6.0f, 2.0f, IVUI::EASE_LINEAR) == 6.0f);
    Check(std::fabs(IVUI::EaseValue(0.0f, 1.0f, 0.5f, IVUI::EASE_IN_OUT_SINE) - 0.5f) < 0.001f);
    Check(!IVUI::EaseValueColor(ImColor(0, 0, 0), ImColor(255, 255, 255), nullptr,
                                1.0f, IVUI::EASE_LINEAR));

    const auto blur = IVUI::MakeBlurEffectConstants({200, 100}, {1.0f, 0.0f}, 7.0f);
    Check(std::fabs(blur.values[0] - 0.005f) < 0.0001f);
    Check(std::fabs(blur.values[1] - 0.01f) < 0.0001f);
    Check(blur.values[2] == 1.0f && blur.values[4] == 7.0f);
    Check(IVUI::HasEffectCapabilities(IVUI::AllEffectCapabilities,
                                      IVUI::RequiredEffectCapabilities(IVUI::EffectId::Glass)));

    TestEffects backend;
    IVUI::EffectRegistry registry(&backend);
    int customCalls = 0;
    const auto customId = static_cast<IVUI::EffectId>(
        static_cast<unsigned>(IVUI::EffectId::CustomFirst) + 7u);
    Check(registry.Register(customId, CustomEffect, &customCalls));
    IVUI::EffectRequest custom{};
    custom.id = customId;
    registry.RenderEffect(custom);
    Check(customCalls == 1 && backend.calls == 1 && backend.last == customId);
    Check(registry.Unregister(customId));

    Counters firstCounters;
    Counters secondCounters;
    auto first = MakeUi(firstCounters, IVUI::Theme{});
    auto second = MakeUi(secondCounters, IVUI::Theme{});
    const IVUI::Theme firstTheme = first->theme().Preset(IVUI::Preset::Peach);
    const IVUI::Theme secondTheme = second->theme().Preset(IVUI::Preset::Cyan);
    first->theme().Mutable() = firstTheme;
    second->theme().Mutable() = secondTheme;
    Check(first->GetImGuiContext() != second->GetImGuiContext());
    Check(first->theme().Current().Accent == firstTheme.Accent);
    Check(second->theme().Current().Accent == secondTheme.Accent);

    Check(first->ProcessEvent({}));
    Check(first->BeginFrame());
    ImGui::Begin("Test");
    bool checked = false;
    int layoutCalls = 0;
    Check(first->layouts().Register("test", std::make_unique<TestLayout>(layoutCalls)));
    Check(first->layouts().Custom("test", [&] { ImGui::TextUnformatted("Custom layout"); }));
    first->widgets().Checkbox("Check", &checked);
    first->widgets().ProgressBar("Progress", 0.5f);
    ImGui::End();
    TestFrame frame;
    Check(first->Render(frame));
    Check(firstCounters.platformFrames == 1 && firstCounters.rendererFrames == 1 &&
          firstCounters.rendererCalls == 1);
    Check(layoutCalls == 1 && first->layouts().Unregister("test"));

    first.reset();
    second.reset();
    Check(firstCounters.platformShutdown == 1 && firstCounters.rendererShutdown == 1);
    Check(secondCounters.platformShutdown == 1 && secondCounters.rendererShutdown == 1);
    return EXIT_SUCCESS;
}
