#include "ImGuiVectorUI/UI.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Layout/AutoSizeChild.h"
#include "ImGuiVectorUI/Layout/ScrollRegion.h"
#include "ImGuiVectorUI/Layout/Section.h"
#include "ImGuiVectorUI/Layout/Table.h"
#include "ImGuiVectorUI/Layout/Tabs.h"
#include "ImGuiVectorUI/Widgets/Button.h"
#include "ImGuiVectorUI/Widgets/Basic.h"
#include "ImGuiVectorUI/Widgets/ColorPicker.h"
#include "ImGuiVectorUI/Widgets/Combo.h"
#include "ImGuiVectorUI/Widgets/HotkeyInput.h"
#include "ImGuiVectorUI/Widgets/Popup.h"
#include "ImGuiVectorUI/Widgets/Slider.h"
#include "ImGuiVectorUI/Widgets/TextInput.h"
#include "ImGuiVectorUI/Widgets/Toggle.h"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <utility>

namespace IVUI
{
    struct UI::Impl
    {
        UIConfig config;
        ImGuiContext* imgui = nullptr;
        ImGuiContext* previousFrameContext = nullptr;
        Context* context = nullptr;
        Context* previousFrameLibraryContext = nullptr;
        std::unique_ptr<EffectRegistry> effectRegistry;
        std::unordered_map<std::string, std::unique_ptr<ILayout>> customLayouts;
        ImFont* regularFont = nullptr;
        ImFont* titleFont = nullptr;
        std::thread::id ownerThread = std::this_thread::get_id();
        bool frameActive = false;
    };

    namespace
    {
        class ContextScope
        {
        public:
            ContextScope(ImGuiContext* imguiContext, Context* libraryContext = nullptr)
                : previous_(ImGui::GetCurrentContext()),
                  previousLibrary_(SetCurrentContext(libraryContext))
            {
                if (imguiContext != previous_)
                    ImGui::SetCurrentContext(imguiContext);
            }
            ~ContextScope()
            {
                SetCurrentContext(previousLibrary_);
                ImGui::SetCurrentContext(previous_);
            }
        private:
            ImGuiContext* previous_ = nullptr;
            Context* previousLibrary_ = nullptr;
        };

        void AssignError(std::string* output, const std::string& value)
        {
            if (output != nullptr)
                *output = value;
        }
    }

    std::unique_ptr<UI> UI::Create(UIConfig config, std::string* error) noexcept
    {
        if (!config.platform || !config.renderer)
        {
            AssignError(error, "IVUI::UI requires platform and renderer backends.");
            return nullptr;
        }

        auto impl = std::make_unique<Impl>();
        impl->config = std::move(config);
        impl->imgui = ImGui::CreateContext();
        if (impl->imgui == nullptr)
        {
            AssignError(error, "ImGui::CreateContext failed.");
            return nullptr;
        }

        const ContextScope scope(impl->imgui);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= impl->config.configFlags;
        ImGui::StyleColorsDark();

        std::string backendError;
        if (!impl->config.platform->Initialize(*impl->imgui, backendError))
        {
            impl->config.platform.reset();
            ImGui::DestroyContext(impl->imgui);
            impl->imgui = nullptr;
            AssignError(error, backendError.empty() ? "Platform backend initialization failed." : backendError);
            return nullptr;
        }
        if (!impl->config.renderer->Initialize(*impl->imgui, backendError))
        {
            impl->config.platform->Shutdown();
            impl->config.renderer.reset();
            impl->config.platform.reset();
            ImGui::DestroyContext(impl->imgui);
            impl->imgui = nullptr;
            AssignError(error, backendError.empty() ? "Renderer backend initialization failed." : backendError);
            return nullptr;
        }

        impl->context = CreateContext();
        Theme initial = impl->config.initialTheme;
        if (initial.Scale <= 0.0f)
            initial = ThemeInjectionPeach();
        SetTargetTheme(initial);
        GetMutableTheme() = initial;
        SetContentScale(std::max(impl->config.contentScale, 0.01f));
        if (impl->config.configureFonts)
            impl->config.configureFonts(io, impl->regularFont, impl->titleFont);
        SetFonts(impl->regularFont, impl->titleFont);
        impl->effectRegistry = std::make_unique<EffectRegistry>(impl->config.effects.get());
        impl->context->effectRenderer = impl->effectRegistry.get();

        auto result = std::unique_ptr<UI>(new UI(std::move(impl)));
        AssignError(error, {});
        return result;
    }

    UI::UI(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
    {
        widgets_.reset(new Widgets(*this));
        layouts_.reset(new Layouts(*this));
        effects_.reset(new Effects(*this));
        themes_.reset(new Themes(*this));
        motion_.reset(new Motion(*this));
    }

    UI::~UI()
    {
        if (!impl_ || !impl_->imgui)
            return;
        ContextScope scope(impl_->imgui, impl_->context);
        if (impl_->frameActive)
            ImGui::EndFrame();
        ClearToasts();
        DestroyContext(impl_->context);
        impl_->context = nullptr;
        if (impl_->config.renderer)
            impl_->config.renderer->Shutdown();
        if (impl_->config.platform)
            impl_->config.platform->Shutdown();
        ImGui::DestroyContext(impl_->imgui);
        impl_->imgui = nullptr;
    }

    void UI::Activate() const noexcept
    {
        if (!impl_)
            return;
        if (impl_->imgui && ImGui::GetCurrentContext() != impl_->imgui)
            ImGui::SetCurrentContext(impl_->imgui);
        if (GetContext() != impl_->context)
            SetCurrentContext(impl_->context);
    }

    bool UI::ProcessEvent(const PlatformEvent& event) noexcept
    {
        if (!impl_ || std::this_thread::get_id() != impl_->ownerThread)
            return false;
        ContextScope scope(impl_->imgui, impl_->context);
        return impl_->config.platform->ProcessEvent(event);
    }

    bool UI::BeginFrame(const UIFrameInfo& info, std::string* error) noexcept
    {
        if (!impl_ || impl_->frameActive || std::this_thread::get_id() != impl_->ownerThread)
        {
            AssignError(error, "IVUI::UI frame is already active or called from the wrong thread.");
            return false;
        }
        impl_->previousFrameContext = ImGui::GetCurrentContext();
        impl_->previousFrameLibraryContext = GetContext();
        ImGui::SetCurrentContext(impl_->imgui);
        SetCurrentContext(impl_->context);
        impl_->config.renderer->NewFrame();
        impl_->config.platform->NewFrame();
        ImGui::NewFrame();
        IVUI::NewFrame();
        if (info.contentScale > 0.0f)
            IVUI::SetContentScale(info.contentScale);
        impl_->frameActive = true;
        AssignError(error, {});
        return true;
    }

    bool UI::Render(const RenderFrame& frame, std::string* error) noexcept
    {
        if (!impl_ || !impl_->frameActive || std::this_thread::get_id() != impl_->ownerThread)
        {
            AssignError(error, "IVUI::UI::Render requires an active frame on the owner thread.");
            return false;
        }
        Activate();
        RenderToasts();
        ImGui::Render();
        std::string renderError;
        const bool rendered = impl_->config.renderer->RenderDrawData(
            *ImGui::GetDrawData(), frame, impl_->effectRegistry.get(), renderError);
        impl_->frameActive = false;
        ImGui::SetCurrentContext(impl_->previousFrameContext);
        SetCurrentContext(impl_->previousFrameLibraryContext);
        impl_->previousFrameContext = nullptr;
        impl_->previousFrameLibraryContext = nullptr;
        AssignError(error, rendered ? std::string{} : renderError);
        return rendered;
    }

    void UI::EndFrameWithoutRender() noexcept
    {
        if (!impl_ || !impl_->frameActive)
            return;
        Activate();
        ImGui::EndFrame();
        impl_->frameActive = false;
        ImGui::SetCurrentContext(impl_->previousFrameContext);
        SetCurrentContext(impl_->previousFrameLibraryContext);
        impl_->previousFrameContext = nullptr;
        impl_->previousFrameLibraryContext = nullptr;
    }

    ImGuiContext* UI::GetImGuiContext() const noexcept { return impl_ ? impl_->imgui : nullptr; }
    bool UI::IsFrameActive() const noexcept { return impl_ && impl_->frameActive; }
    bool UI::WantCaptureHotkey() const noexcept
    {
        if (!impl_) return false;
        ContextScope scope(impl_->imgui, impl_->context);
        return IVUI::WantCaptureHotkey();
    }
    bool UI::WantTextInput() const noexcept
    {
        if (!impl_) return false;
        ContextScope scope(impl_->imgui, impl_->context);
        return ImGui::GetIO().WantTextInput;
    }
    UI::Widgets& UI::widgets() noexcept { return *widgets_; }
    UI::Layouts& UI::layouts() noexcept { return *layouts_; }
    UI::Effects& UI::effects() noexcept { return *effects_; }
    UI::Themes& UI::theme() noexcept { return *themes_; }
    UI::Motion& UI::motion() noexcept { return *motion_; }

#define IVUI_WIDGET_CALL(expression) owner_.Activate(); return (expression)
    bool UI::Widgets::Button(const char* l, ImVec2 s, bool p) { IVUI_WIDGET_CALL(IVUI::Button(l, s, p)); }
    bool UI::Widgets::Toggle(const char* l, bool* v) { IVUI_WIDGET_CALL(IVUI::Toggle(l, v)); }
    bool UI::Widgets::Checkbox(const char* l, bool* v) { IVUI_WIDGET_CALL(IVUI::Checkbox(l, v)); }
    bool UI::Widgets::RadioButton(const char* l, bool a) { IVUI_WIDGET_CALL(IVUI::RadioButton(l, a)); }
    bool UI::Widgets::RadioButton(const char* l, int* v, int b) { IVUI_WIDGET_CALL(IVUI::RadioButton(l, v, b)); }
    bool UI::Widgets::SliderFloat(const char* l, float* v, float mn, float mx, const char* f) { IVUI_WIDGET_CALL(IVUI::SliderFloat(l, v, mn, mx, f)); }
    int UI::Widgets::SliderSegments(const char* l, int* v, int n, const char* f) { IVUI_WIDGET_CALL(IVUI::SliderSegments(l, v, n, f)); }
    int UI::Widgets::SliderLabeled(const char* l, int* v, std::initializer_list<const char*> o, const char* f) { IVUI_WIDGET_CALL(IVUI::SliderLabeled(l, v, o, f)); }
    bool UI::Widgets::Combo(const char* l, int* v, std::initializer_list<const char*> i) { IVUI_WIDGET_CALL(IVUI::Combo(l, v, i)); }
    bool UI::Widgets::ComboMulti(const char* l, std::initializer_list<ComboItem> i) { IVUI_WIDGET_CALL(IVUI::ComboMulti(l, i)); }
    bool UI::Widgets::ColorPicker(const char* l, ImVec4* c) { IVUI_WIDGET_CALL(IVUI::ColorPicker(l, c)); }
    bool UI::Widgets::TextInput(const char* l, char* b, std::size_t n, const char* h) { IVUI_WIDGET_CALL(IVUI::TextInput(l, b, n, h)); }
    bool UI::Widgets::InputTextMultiline(const char* l, char* b, std::size_t n, ImVec2 s, ImGuiInputTextFlags f) { IVUI_WIDGET_CALL(IVUI::InputTextMultiline(l, b, n, s, f)); }
    bool UI::Widgets::SearchInput(const char* l, char* b, std::size_t n, const char* h, float w) { IVUI_WIDGET_CALL(IVUI::SearchInput(l, b, n, h, w)); }
    bool UI::Widgets::InputInt(const char* l, int* v, int mn, int mx, int st) { IVUI_WIDGET_CALL(IVUI::InputInt(l, v, mn, mx, st)); }
    bool UI::Widgets::InputFloat(const char* l, float* v, float mn, float mx, float st, const char* f) { IVUI_WIDGET_CALL(IVUI::InputFloat(l, v, mn, mx, st, 0.0f, f)); }
    bool UI::Widgets::HotkeyInput(const char* l, int* k) { IVUI_WIDGET_CALL(IVUI::HotkeyInput(l, k)); }
    bool UI::Widgets::KeybindRow(const char* l, Keybind* k) { IVUI_WIDGET_CALL(IVUI::KeybindRow(l, k)); }
    bool UI::Widgets::Selectable(const char* l, bool s, ImGuiSelectableFlags f, ImVec2 z) { IVUI_WIDGET_CALL(IVUI::Selectable(l, s, f, z)); }
    bool UI::Widgets::Selectable(const char* l, bool* s, ImGuiSelectableFlags f, ImVec2 z) { IVUI_WIDGET_CALL(IVUI::Selectable(l, s, f, z)); }
    void UI::Widgets::ProgressBar(const char* l, float v, ImVec2 s, const char* o) { owner_.Activate(); IVUI::ProgressBar(l, v, s, o); }
    void UI::Widgets::Spinner(const char* l, float r, float t) { owner_.Activate(); IVUI::Spinner(l, r, t); }
    void UI::Widgets::OpenConfirm(const char* id) { owner_.Activate(); IVUI::OpenConfirm(id); }
    bool UI::Widgets::ConfirmPopup(const char* id, const char* m, const char* y, const char* n) { IVUI_WIDGET_CALL(IVUI::ConfirmPopup(id, m, y, n)); }
    void UI::Widgets::Toast(ToastType t, const char* title, const char* body, float seconds) { owner_.Activate(); IVUI::Toast(t, title, body, seconds); }
    void UI::Widgets::ClearToasts() { owner_.Activate(); IVUI::ClearToasts(); }
#undef IVUI_WIDGET_CALL

    void UI::Layouts::Panel(const char* l, ImVec2 p, ImVec2 s, const std::function<void()>& b, ImGuiWindowFlags f) { owner_.Activate(); IVUI::Panel(l, p, s, b, f); }
    void UI::Layouts::Panel(const char* l, ImVec2 p, ImVec2 s, const std::function<void()>& b, PanelHeader* h, ImGuiWindowFlags f) { owner_.Activate(); IVUI::Panel(l, p, s, b, h, f); }
    bool UI::Layouts::Section(const char* l, bool* o, const std::function<void()>& b) { owner_.Activate(); return IVUI::Section(l, o, b); }
    bool UI::Layouts::Tab(const char* l, IconType i, int n, int* a) { owner_.Activate(); return IVUI::Tab(l, i, n, a); }
    bool UI::Layouts::BeginTable(const char* id, int n, ImVec2 s, ImGuiTableFlags f) { owner_.Activate(); return IVUI::BeginTable(id, n, s, f); }
    void UI::Layouts::EndTable() { owner_.Activate(); IVUI::EndTable(); }
    float UI::Layouts::AnimatedChild(const char* id, ImVec2 s, float w, const std::function<void()>& b) { owner_.Activate(); return IVUI::AnimatedChild(id, s, w, b); }
    void UI::Layouts::OverlayScrollbar() { owner_.Activate(); IVUI::OverlayScrollbar(); }
    void UI::Layouts::StatusList(ImVec2 a, const char* const* l, int n, const StatusListOptions& o) { owner_.Activate(); IVUI::StatusList(a, l, n, o); }
    RowScope UI::Layouts::Row(float s) { owner_.Activate(); return RowScope(s); }
    ColumnScope UI::Layouts::Column(float s) { owner_.Activate(); return ColumnScope(s); }
    GridScope UI::Layouts::Grid(const char* id, int n, ImVec2 s, ImGuiTableFlags f) { owner_.Activate(); return GridScope(id, n, s, f); }
    VirtualListScope UI::Layouts::VirtualList(int n, float h) { owner_.Activate(); return VirtualListScope(n, h); }
    void UI::Layouts::Spacer(float h) { owner_.Activate(); IVUI::Spacer(h); }
    void UI::Layouts::Spacer(ImVec2 s) { owner_.Activate(); IVUI::Spacer(s); }
    void UI::Layouts::Separator(const char* l) { owner_.Activate(); IVUI::Separator(l); }
    int UI::Layouts::ResponsiveColumnCount(float w, int m, float a, float s) { owner_.Activate(); return IVUI::ResponsiveColumnCount(w, m, a, s); }
    bool UI::Layouts::Register(const std::string& id, std::unique_ptr<ILayout> layout)
    {
        if (id.empty() || !layout) return false;
        owner_.impl_->customLayouts.insert_or_assign(id, std::move(layout));
        return true;
    }
    bool UI::Layouts::Unregister(const std::string& id)
    {
        return owner_.impl_->customLayouts.erase(id) != 0;
    }
    bool UI::Layouts::Custom(const std::string& id, const std::function<void()>& body)
    {
        owner_.Activate();
        const auto found = owner_.impl_->customLayouts.find(id);
        if (found == owner_.impl_->customLayouts.end()) return false;
        found->second->Render(owner_, body);
        return true;
    }

    EffectCapabilities UI::Effects::Capabilities() const noexcept
    {
        IEffectRenderer* renderer = Renderer();
        return renderer ? renderer->GetCapabilities() : EffectCapabilities::None;
    }
    bool UI::Effects::Submit(const EffectRequest& request)
    {
        owner_.Activate();
        IEffectRenderer* renderer = Renderer();
        if (!renderer || !HasEffectCapabilities(renderer->GetCapabilities(), RequiredEffectCapabilities(request.id)))
            return false;
        SubmitEffect(*renderer, request);
        return true;
    }
    bool UI::Effects::Glass(const GlassEffectParams& params) { return Submit(EffectRequest::Glass(params)); }
    bool UI::Effects::Register(EffectId id, CustomEffectCallback callback, void* userData)
    {
        return owner_.impl_->effectRegistry->Register(id, callback, userData);
    }
    bool UI::Effects::Unregister(EffectId id) { return owner_.impl_->effectRegistry->Unregister(id); }
    IEffectRenderer* UI::Effects::Renderer() const noexcept { return owner_.impl_->effectRegistry.get(); }

    const Theme& UI::Themes::Current() const { owner_.Activate(); return IVUI::GetTheme(); }
    Theme& UI::Themes::Mutable() { owner_.Activate(); return IVUI::GetMutableTheme(); }
    void UI::Themes::SetTarget(const Theme& t) { owner_.Activate(); IVUI::SetTargetTheme(t); }
    void UI::Themes::Push(const Theme& t) { owner_.Activate(); IVUI::PushTheme(t); }
    void UI::Themes::Pop() { owner_.Activate(); IVUI::PopTheme(); }
    void UI::Themes::SetFonts(ImFont* r, ImFont* t) { owner_.Activate(); IVUI::SetFonts(r, t); }
    void UI::Themes::SetContentScale(float s) { owner_.Activate(); IVUI::SetContentScale(s); }
    float UI::Themes::ContentScale() const { owner_.Activate(); return IVUI::GetContentScale(); }
    Theme UI::Themes::Preset(IVUI::Preset p) const { return IVUI::GetPreset(p); }
    Theme UI::Themes::StylePreset(IVUI::Style s) const { return IVUI::GetStyle(s); }

    float UI::Motion::Approach(ImGuiID id, float target, float speed) { owner_.Activate(); return IVUI::Approach(id, target, speed); }
    float UI::Motion::Toggle(ImGuiID id, bool on, float speed, EasingType e) { owner_.Activate(); return IVUI::Toggle01(id, on, speed, e); }
    float UI::Motion::Transition(ImGuiID id, int key, float speed, EasingType e) { owner_.Activate(); return IVUI::Transition(id, key, speed, e); }
}
