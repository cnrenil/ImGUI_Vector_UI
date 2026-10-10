#include "ImGuiVectorUI/UI.h"
#include "ImGuiVectorUI/Drawing/Effects.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <source_location>
#include <string>

namespace
{
    void Check(bool condition,
               const std::source_location& location = std::source_location::current())
    {
        if (!condition)
        {
            std::fprintf(stderr, "Check failed at %s:%u\n",
                         location.file_name(), location.line());
            std::exit(EXIT_FAILURE);
        }
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
            for (const ImDrawList* drawList : data.CmdLists)
                for (const ImDrawCmd& command : drawList->CmdBuffer)
                {
                    if (command.UserCallback &&
                        command.UserCallback != ImDrawCallback_ResetRenderState)
                        command.UserCallback(drawList, &command);
                }
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
        void RenderItemEffects(const IVUI::ItemEffectRequest& request,
                               const IVUI::ItemEffectDrawData&) noexcept override
        {
            itemLayers = static_cast<int>(request.layerCount);
            ++itemCalls;
        }
        IVUI::EffectId last = IVUI::EffectId::BackdropBlur;
        int calls = 0;
        int itemCalls = 0;
        int itemLayers = 0;
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

    bool SampleColor(ImVec2 screenPosition, ImVec4& outputColor, void* userData)
    {
        Check(screenPosition.x == 120.0f && screenPosition.y == 80.0f);
        ++*static_cast<int*>(userData);
        outputColor = ImVec4(0.2f, 0.4f, 0.6f, 0.8f);
        return true;
    }

    std::unique_ptr<IVUI::UI> MakeUi(Counters& counters, IVUI::Theme theme,
                                     std::unique_ptr<IVUI::IEffectRenderer> effects = {})
    {
        IVUI::UIConfig config;
        config.platform = std::make_unique<TestPlatform>(counters);
        config.renderer = std::make_unique<TestRenderer>(counters);
        config.initialTheme = theme;
        config.effects = std::move(effects);
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

    IVUI::ThemeDefinition customDefinition;
    customDefinition.palette = IVUI::ThemePalette::Light();
    customDefinition.metrics = IVUI::ThemeMetrics::Compact();
    customDefinition.shape.controlRounding = 0.0f;
    const IVUI::Theme customTheme = IVUI::MakeTheme(customDefinition);
    Check(customTheme.Surface0 == customDefinition.palette.surface);
    Check(customTheme.Accent == customDefinition.palette.accent);
    Check(customTheme.Success == customDefinition.palette.success);
    Check(customTheme.ControlHeight == customDefinition.metrics.controlHeight);
    Check(customTheme.RoundingControl == 0.0f);
    Check(IVUI::MakeTheme(IVUI::ColorScheme::Light, IVUI::Density::Compact).Text ==
          customDefinition.palette.text);

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
    auto testEffects = std::make_unique<TestEffects>();
    TestEffects* effectObserver = testEffects.get();
    auto first = MakeUi(firstCounters, IVUI::Theme{}, std::move(testEffects));
    auto second = MakeUi(secondCounters, IVUI::Theme{});
    const IVUI::Theme firstTheme = first->theme().Preset(IVUI::Preset::Peach);
    const IVUI::Theme secondTheme = second->theme().Preset(IVUI::Preset::Cyan);
    const IVUI::Theme vapeTheme = first->theme().StylePreset(IVUI::Style::Vape);
    const IVUI::Theme classicTheme = first->theme().StylePreset(IVUI::Style::ImGuiClassic);
    const IVUI::Theme iosTheme = first->theme().StylePreset(IVUI::Style::iOSGlass);
    Check(vapeTheme.ForElement(IVUI::ElementRole::Button).effects.layerCount > 0);
    Check(vapeTheme.ForElement(IVUI::ElementRole::Button).effects.layerCount ==
          classicTheme.ForElement(IVUI::ElementRole::Button).effects.layerCount);
    Check(classicTheme.WidgetGlow == 0.0f);
    Check(iosTheme.Notifications.style == IVUI::NotificationStyle::DynamicIsland);
    Check(iosTheme.Notifications.placement == IVUI::NotificationPlacement::TopCenter);
    Check(iosTheme.ForElement(IVUI::ElementRole::Notification).motion.scale < 1.0f);
    Check(vapeTheme.ForElement(IVUI::ElementRole::Button).motion.offset.x !=
          classicTheme.ForElement(IVUI::ElementRole::Button).motion.offset.x);
    IVUI::Theme scaledTheme = vapeTheme;
    scaledTheme.Scale = 2.0f;
    const IVUI::ElementVisualStyle resolvedButton =
        scaledTheme.ResolveElement(IVUI::ElementRole::Button);
    Check(resolvedButton.motion.offset.x ==
          vapeTheme.ForElement(IVUI::ElementRole::Button).motion.offset.x * 2.0f);
    Check(resolvedButton.effects.layers[0].sigma ==
          vapeTheme.ForElement(IVUI::ElementRole::Button).effects.layers[0].sigma * 2.0f);
    first->theme().Mutable() = firstTheme;
    second->theme().Mutable() = secondTheme;
    Check(first->GetImGuiContext() != second->GetImGuiContext());
    Check(first->theme().Current().Accent == firstTheme.Accent);
    Check(second->theme().Current().Accent == secondTheme.Accent);

    Check(first->ProcessEvent({}));
    Check(first->BeginFrame());
    ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_Always);
    ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    bool checked = false;
    int layoutCalls = 0;
    int samplerCalls = 0;
    first->widgets().SetColorPickerEyedropper(SampleColor, &samplerCalls);
    Check(IVUI::GetColorPickerState() != nullptr);
    Check(IVUI::GetColorPickerState()->eyedropper == SampleColor);
    Check(IVUI::AddColorPickerFavorite(ImVec4(0.1f, 0.2f, 0.3f, 1.0f)));
    Check(IVUI::GetColorPickerState()->favorites.size() == 1);
    ImVec4 pickerColor(0.8f, 0.2f, 0.1f, 0.7f);
    Check(!first->widgets().ColorPicker("Picker", &pickerColor,
                                       {.showHistory = false, .showFavorites = true,
                                        .showRelated = true, .showEyedropper = true}));
    Check(first->layouts().Register("test", std::make_unique<TestLayout>(layoutCalls)));
    Check(first->layouts().Custom("test", [&] { ImGui::TextUnformatted("Custom layout"); }));
    first->widgets().Checkbox("Check", &checked);
    first->widgets().ProgressBar("Progress", 0.5f);
    first->widgets().Button("Options button", {.size = ImVec2(120.0f, 28.0f), .prominent = true});
    IVUI::AnimatedTextOptions animatedText;
    animatedText.animation = IVUI::TextAnimation::Rainbow | IVUI::TextAnimation::Wave;
    first->widgets().AnimatedText("Animated##test", animatedText);
    IVUI::ItemEffectRequest itemRequest = IVUI::ItemEffectRequest::From(
        IVUI::ItemEffectLayer::Glow({1.0f, 0.2f, 0.3f, 1.0f}, 5.0f, 0.8f));
    {
        auto effect = first->effects().BeginItem(itemRequest);
        Check(static_cast<bool>(effect));
        const ImVec2 position = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            position, ImVec2(position.x + 40.0f, position.y + 16.0f), IM_COL32_WHITE);
    }
    IVUI::NotificationOptions notificationOptions;
    notificationOptions.style = IVUI::NotificationStyle::DynamicIsland;
    notificationOptions.placement = IVUI::NotificationPlacement::Custom;
    notificationOptions.position = ImVec2(24.0f, 24.0f);
    notificationOptions.duration = 0.0f;
    notificationOptions.effects = IVUI::ItemEffectRequest{};
    IVUI::ItemMotionStyle notificationMotion = IVUI::ItemMotionStyle::Fade(0.01f);
    notificationMotion.animateInitial = false;
    notificationOptions.motion = notificationMotion;
    const IVUI::NotificationHandle notification = first->widgets().Notify(
        "Queued", "Per-item position and style", notificationOptions);
    Check(notification.value != 0);
    Check(first->widgets().SetNotificationProgress(notification, 0.4f));
    IVUI::ItemMotionStyle motionStyle = IVUI::ItemMotionStyle::FadeSlide(
        ImVec2(18.0f, 0.0f), 1.0f);
    motionStyle.scale = 1.0f;
    ImDrawList* motionDrawList = ImGui::GetWindowDrawList();
    const int motionVertexStart = motionDrawList->VtxBuffer.Size;
    auto motion = first->motion().BeginItem("motion_test", true, motionStyle);
    Check(static_cast<bool>(motion) && motion.Progress() > 0.0f && motion.Progress() < 1.0f);
    const ImVec2 motionPosition = ImGui::GetCursorScreenPos();
    motionDrawList->AddRectFilled(
        motionPosition, ImVec2(motionPosition.x + 32.0f, motionPosition.y + 12.0f),
        IM_COL32_WHITE);
    motion.End();
    Check(motionDrawList->VtxBuffer.Size > motionVertexStart);
    Check(motionDrawList->VtxBuffer[motionVertexStart].pos.x > motionPosition.x + 1.0f);
    Check(((motionDrawList->VtxBuffer[motionVertexStart].col >> IM_COL32_A_SHIFT) & 0xffu) < 255u);
    IVUI::ItemMotionStyle exitStyle = IVUI::ItemMotionStyle::Fade(1.0f);
    exitStyle.enterDuration = 0.001f;
    {
        auto entered = first->motion().BeginItem("exit_motion_test", true, exitStyle);
        Check(static_cast<bool>(entered) && entered.Progress() == 1.0f);
    }
    {
        auto exiting = first->motion().BeginItem("exit_motion_test", false, exitStyle);
        Check(static_cast<bool>(exiting) && exiting.Progress() < 1.0f &&
              exiting.Progress() > 0.99f);
    }
    ImGui::End();
    TestFrame frame;
    Check(first->Render(frame));
    Check(firstCounters.platformFrames == 1 && firstCounters.rendererFrames == 1 &&
          firstCounters.rendererCalls == 1);
    Check(layoutCalls == 1 && first->layouts().Unregister("test"));
    Check(effectObserver->itemCalls == 1 && effectObserver->itemLayers == 1);
    Check(samplerCalls == 0);

    Check(second->BeginFrame());
    Check(IVUI::GetColorPickerState() != nullptr);
    Check(IVUI::GetColorPickerState()->favorites.empty());
    second->EndFrameWithoutRender();

    IVUI::NotificationOptions duplicateOptions;
    duplicateOptions.dedupeKey = "core-test";
    const IVUI::NotificationHandle duplicate = first->widgets().Notify(
        "Updated", "Same handle", duplicateOptions);
    Check(duplicate.value != 0);
    const IVUI::NotificationHandle duplicateAgain = first->widgets().Notify(
        "Deduplicated", "Same key", duplicateOptions);
    Check(duplicateAgain == duplicate);
    IVUI::NotificationUpdate update;
    update.title = "Changed";
    update.body = "Updated body";
    Check(first->widgets().UpdateNotification(duplicate, update));
    Check(first->widgets().CloseNotification(duplicate));
    Check(first->widgets().CloseNotification(notification));

    Check(first->BeginFrame());
    const IVUI::ContentBlock duplicateBlocks[] = {
        {"duplicate", "One", [](IVUI::UI&) {}},
        {"duplicate", "Two", [](IVUI::UI&) {}}
    };
    Check(!first->layouts().Content("invalid-layout", duplicateBlocks));
    ImGui::SetNextWindowSize(ImVec2(240.0f, 160.0f), ImGuiCond_Always);
    ImGui::Begin("Late scopes", nullptr, ImGuiWindowFlags_NoSavedSettings);
    auto lateEffect = first->effects().BeginItem(itemRequest);
    auto lateMotion = first->motion().BeginItem("late_motion", true, motionStyle);
    ImDrawList* lateDrawList = ImGui::GetWindowDrawList();
    const int lateVertexStart = lateDrawList->VtxBuffer.Size;
    const ImVec2 latePosition = ImGui::GetCursorScreenPos();
    lateDrawList->AddRectFilled(
        latePosition, ImVec2(latePosition.x + 20.0f, latePosition.y + 10.0f),
        IM_COL32_WHITE);
    const ImVec2 unmodifiedLatePosition = lateDrawList->VtxBuffer[lateVertexStart].pos;
    ImGui::End();
    Check(first->Render(frame));
    lateMotion.End();
    lateEffect.End();
    Check(effectObserver->itemCalls == 1);
    Check(lateDrawList->VtxBuffer[lateVertexStart].pos.x == unmodifiedLatePosition.x);

    Check(first->BeginFrame());
    ImGui::SetNextWindowSize(ImVec2(240.0f, 160.0f), ImGuiCond_Always);
    ImGui::Begin("Stale scope", nullptr, ImGuiWindowFlags_NoSavedSettings);
    auto staleScope = first->effects().BeginItem(itemRequest);
    auto staleMotion = first->motion().BeginItem("stale_motion", true, motionStyle);
    const ImVec2 stalePosition = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        stalePosition, ImVec2(stalePosition.x + 20.0f, stalePosition.y + 10.0f),
        IM_COL32_WHITE);
    ImGui::End();
    first->EndFrameWithoutRender();

    Check(first->BeginFrame());
    staleScope.End();
    staleMotion.End();
    ImGui::SetNextWindowSize(ImVec2(240.0f, 160.0f), ImGuiCond_Always);
    ImGui::Begin("Fresh scope", nullptr, ImGuiWindowFlags_NoSavedSettings);
    auto freshScope = first->effects().BeginItem(itemRequest);
    Check(static_cast<bool>(freshScope));
    const ImVec2 freshPosition = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        freshPosition, ImVec2(freshPosition.x + 20.0f, freshPosition.y + 10.0f),
        IM_COL32_WHITE);
    freshScope.End();
    ImGui::End();
    first->EndFrameWithoutRender();

    first.reset();
    second.reset();
    Check(firstCounters.platformShutdown == 1 && firstCounters.rendererShutdown == 1);
    Check(secondCounters.platformShutdown == 1 && secondCounters.rendererShutdown == 1);
    return EXIT_SUCCESS;
}
