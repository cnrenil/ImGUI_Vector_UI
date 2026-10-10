/** @file UI.h @brief Owning object-oriented entry point for ImGuiVectorUI. */
#pragma once

#include "imgui.h"

#include "ImGuiVectorUI/Core/Types.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Layout/Flex.h"
#include "ImGuiVectorUI/Layout/Panel.h"
#include "ImGuiVectorUI/Layout/StatusChips.h"
#include "ImGuiVectorUI/Layout/Workbench.h"
#include "ImGuiVectorUI/Layout/Content.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Theme/Design.h"
#include "ImGuiVectorUI/Widgets/Button.h"
#include "ImGuiVectorUI/Widgets/ColorPicker.h"
#include "ImGuiVectorUI/Widgets/Keybind.h"
#include "ImGuiVectorUI/Widgets/Notification.h"
#include "ImGuiVectorUI/Widgets/AnimatedText.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>

namespace IVUI
{
    class UI;

    class ILayout
    {
    public:
        virtual ~ILayout() = default;
        virtual void Render(UI& ui, const std::function<void()>& body) = 0;
    };

    struct PlatformEvent
    {
        const void* native = nullptr;
    };

    struct RenderFrame
    {
        virtual ~RenderFrame() = default;
    };

    class IPlatformBackend
    {
    public:
        virtual ~IPlatformBackend() = default;
        virtual bool Initialize(ImGuiContext& context, std::string& error) noexcept = 0;
        virtual void Shutdown() noexcept = 0;
        virtual void NewFrame() noexcept = 0;
        virtual bool ProcessEvent(const PlatformEvent& event) noexcept = 0;
    };

    class IRendererBackend
    {
    public:
        virtual ~IRendererBackend() = default;
        virtual bool Initialize(ImGuiContext& context, std::string& error) noexcept = 0;
        virtual void Shutdown() noexcept = 0;
        virtual void NewFrame() noexcept = 0;
        virtual bool RenderDrawData(ImDrawData& drawData, const RenderFrame& frame,
                                    IEffectRenderer* effects, std::string& error) noexcept = 0;
    };

    struct UIConfig
    {
        std::unique_ptr<IPlatformBackend> platform;
        std::unique_ptr<IRendererBackend> renderer;
        std::unique_ptr<IEffectRenderer> effects;
        Theme initialTheme = Theme{};
        float contentScale = 1.0f;
        ImGuiConfigFlags configFlags = ImGuiConfigFlags_NavEnableKeyboard;
        std::function<void(ImGuiIO&, ImFont*&, ImFont*&)> configureFonts;
        MotionSettings motion{};
        bool syncImGuiStyle = true;
    };

    struct UIFrameInfo
    {
        float contentScale = 0.0f;
    };

    class UI final
    {
    public:
        class Widgets;
        class Layouts;
        class Effects;
        class Themes;
        class Motion;
        class Elements;

        static std::unique_ptr<UI> Create(UIConfig config, std::string* error = nullptr) noexcept;

        ~UI();
        UI(UI&&) = delete;
        UI& operator=(UI&&) = delete;
        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;

        bool ProcessEvent(const PlatformEvent& event) noexcept;
        bool BeginFrame(const UIFrameInfo& info = {}, std::string* error = nullptr) noexcept;
        bool Render(const RenderFrame& frame, std::string* error = nullptr) noexcept;
        void EndFrameWithoutRender() noexcept;

        ImGuiContext* GetImGuiContext() const noexcept;
        bool IsFrameActive() const noexcept;
        bool WantCaptureHotkey() const noexcept;
        bool WantTextInput() const noexcept;

        Widgets& widgets() noexcept;
        Layouts& layouts() noexcept;
        Effects& effects() noexcept;
        Themes& theme() noexcept;
        Motion& motion() noexcept;
        Elements& elements() noexcept;

    private:
        struct Impl;
        explicit UI(std::unique_ptr<Impl> impl) noexcept;
        void Activate() const noexcept;
        std::unique_ptr<Impl> impl_;
        std::unique_ptr<Widgets> widgets_;
        std::unique_ptr<Layouts> layouts_;
        std::unique_ptr<Effects> effects_;
        std::unique_ptr<Themes> themes_;
        std::unique_ptr<Motion> motion_;
        std::unique_ptr<Elements> elements_;
    };

    class UI::Widgets final
    {
    public:
        bool Button(const char* label, const ButtonOptions& options = {});
        bool Toggle(const char* label, bool* value);
        bool Checkbox(const char* label, bool* value);
        bool RadioButton(const char* label, bool active);
        bool RadioButton(const char* label, int* value, int buttonValue);
        bool SliderFloat(const char* label, float* value, float min, float max,
                         const char* format = "%.1f");
        int SliderSegments(const char* label, int* value, int segments,
                           const char* format = "%d");
        int SliderLabeled(const char* label, int* value,
                          std::initializer_list<const char*> options,
                          const char* format = "%s");
        bool Combo(const char* label, int* index, std::initializer_list<const char*> items);
        bool ComboMulti(const char* label, std::initializer_list<ComboItem> items);
        bool ColorPicker(const char* label, ImVec4* color);
        bool ColorPicker(const char* label, ImVec4* color,
                         const ColorPickerOptions& options);
        void SetColorPickerEyedropper(ColorPickerEyedropperCallback callback,
                                      void* userData = nullptr) noexcept;
        bool TextInput(const char* label, char* buffer, std::size_t capacity,
                       const char* hint = nullptr);
        bool InputTextMultiline(const char* label, char* buffer, std::size_t capacity,
                                ImVec2 size = {}, ImGuiInputTextFlags flags = 0);
        bool SearchInput(const char* label, char* buffer, std::size_t capacity,
                         const char* hint = "Search", float width = 0.0f);
        bool InputInt(const char* label, int* value, int min, int max, int step = 1);
        bool InputFloat(const char* label, float* value, float min, float max,
                        float step = 0.0f, const char* format = "%.3f");
        bool HotkeyInput(const char* label, int* key);
        bool KeybindRow(const char* label, Keybind* keybind);
        bool Selectable(const char* label, bool selected = false,
                        ImGuiSelectableFlags flags = 0, ImVec2 size = {});
        bool Selectable(const char* label, bool* selected,
                        ImGuiSelectableFlags flags = 0, ImVec2 size = {});
        void ProgressBar(const char* label, float fraction, ImVec2 size = {},
                         const char* overlay = nullptr);
        void Spinner(const char* label, float radius = 0.0f, float thickness = 0.0f);
        void AnimatedText(const char* label, const AnimatedTextOptions& options = {});
        void OpenConfirm(const char* id);
        bool ConfirmPopup(const char* id, const char* message,
                          const char* yes = "Confirm", const char* no = "Cancel");
        NotificationHandle Notify(const char* title, const char* body = nullptr,
                                  const NotificationOptions& options = {});
        bool UpdateNotification(NotificationHandle handle, const NotificationUpdate& update);
        bool CloseNotification(NotificationHandle handle);
        bool SetNotificationProgress(NotificationHandle handle, float progress);
        void ClearNotifications();

    private:
        friend class UI;
        explicit Widgets(UI& owner) : owner_(owner) {}
        UI& owner_;
    };

    class UI::Layouts final
    {
    public:
        bool Content(const char* id, std::span<const ContentBlock> blocks,
                     const ContentLayoutOptions& options = {});
        void Workbench(const char* id, const WorkbenchOptions& options,
                       const std::function<void(bool compact)>& navigation,
                       const std::function<void()>& body);
        void Panel(const char* label, ImVec2 position, ImVec2 maxSize,
                   const std::function<void()>& body, ImGuiWindowFlags flags = 0);
        void Panel(const char* label, ImVec2 position, ImVec2 maxSize,
                   const std::function<void()>& body, PanelHeader* header,
                   ImGuiWindowFlags flags = 0);
        bool Section(const char* label, bool* open, const std::function<void()>& body);
        bool Tab(const char* label, IconType icon, int index, int* active);
        bool BeginTable(const char* id, int columns, ImVec2 outerSize = {},
                        ImGuiTableFlags flags = 0);
        void EndTable();
        float AnimatedChild(const char* id, ImVec2 outerSize, float innerWidth,
                            const std::function<void()>& body);
        void OverlayScrollbar();
        void StatusList(ImVec2 anchorTopRight, const char* const* labels, int count,
                        const StatusListOptions& options = {});
        RowScope Row(float spacing = -1.0f);
        ColumnScope Column(float spacing = -1.0f);
        GridScope Grid(const char* id, int columns, ImVec2 outerSize = {},
                       ImGuiTableFlags flags = 0);
        VirtualListScope VirtualList(int itemCount, float itemHeight = -1.0f);
        void Spacer(float height);
        void Spacer(ImVec2 size);
        void Separator(const char* label = nullptr);
        int ResponsiveColumnCount(float minColumnWidth, int maxColumns = 0,
                                  float availableWidth = 0.0f, float spacing = -1.0f);
        bool Register(const std::string& id, std::unique_ptr<ILayout> layout);
        bool Unregister(const std::string& id);
        bool Custom(const std::string& id, const std::function<void()>& body);

    private:
        friend class UI;
        explicit Layouts(UI& owner) : owner_(owner) {}
        UI& owner_;
    };

    class UI::Effects final
    {
    public:
        EffectCapabilities Capabilities() const noexcept;
        bool Submit(const EffectRequest& request);
        bool Glass(const GlassEffectParams& params);
        ItemEffectScope BeginItem(const ItemEffectRequest& request);
        bool Register(EffectId id, CustomEffectCallback callback, void* userData = nullptr);
        bool Unregister(EffectId id);
        IEffectRenderer* Renderer() const noexcept;

    private:
        friend class UI;
        explicit Effects(UI& owner) : owner_(owner) {}
        UI& owner_;
    };

    class UI::Themes final
    {
    public:
        const Theme& Current() const;
        Theme& Mutable();
        void SetTarget(const Theme& value);
        void Push(const Theme& value);
        void Pop();
        void SetFonts(ImFont* regular, ImFont* title);
        void SetContentScale(float scale);
        float ContentScale() const;
        Theme Preset(Preset preset) const;
        Theme StylePreset(Style style) const;

    private:
        friend class UI;
        explicit Themes(UI& owner) : owner_(owner) {}
        UI& owner_;
    };

    class UI::Motion final
    {
    public:
        void Configure(const MotionSettings& settings);
        MotionSettings Settings() const;
        float Approach(ImGuiID id, float target, float speed = 12.0f);
        float Toggle(ImGuiID id, bool on, float speed = 8.0f,
                     EasingType easing = EASE_IN_OUT_CIRC);
        float Transition(ImGuiID id, int key, float speed = 6.0f,
                          EasingType easing = EASE_OUT_CUBIC);
        ItemMotionScope BeginItem(ImGuiID id, bool visible,
                                  const ItemMotionStyle& style = {});
        ItemMotionScope BeginItem(const char* label, bool visible,
                                  const ItemMotionStyle& style = {});

    private:
        friend class UI;
        explicit Motion(UI& owner) : owner_(owner) {}
        UI& owner_;
    };

    class UI::Elements final
    {
    public:
        ElementScope Begin(ImDrawList* drawList, ImGuiID id, bool visible,
                           const ElementOptions& options = {});
        ElementScope Begin(ImGuiID id, bool visible, const ElementOptions& options = {});
        ElementScope Begin(const char* label, bool visible, const ElementOptions& options = {});

    private:
        friend class UI;
        explicit Elements(UI& owner) : owner_(owner) {}
        UI& owner_;
    };
}
