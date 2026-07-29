#include "ImGuiVectorUI/Widgets/Basic.h"

#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"

#include <cmath>
#include <utility>

namespace IVUI
{
    namespace
    {
        const char* SafeLabel(const char* label, const char* fallback)
        {
            return label && label[0] != '\0' ? label : fallback;
        }

        float SafeScale(const Theme& theme)
        {
            return std::isfinite(theme.Scale) && theme.Scale > 0.0f ? theme.Scale : 1.0f;
        }

        ImVec2 NonNegativeSize(ImVec2 size)
        {
            if (!std::isfinite(size.x) || size.x < 0.0f)
                size.x = 0.0f;
            if (!std::isfinite(size.y) || size.y < 0.0f)
                size.y = 0.0f;
            return size;
        }

        class ControlStyle
        {
        public:
            explicit ControlStyle(bool selectedText = false)
            {
                const Theme& theme = GetTheme();
                const float scale = SafeScale(theme);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme.AccentMuted);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, theme.Accent);
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, theme.Accent);
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, theme.AccentActive);
                ImGui::PushStyleColor(ImGuiCol_Button, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.AccentHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.AccentActive);
                ImGui::PushStyleColor(ImGuiCol_Header, theme.Accent);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selectedText ? theme.AccentHover : theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme.AccentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, selectedText ? theme.OnAccent : theme.Text);
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, theme.TextDisabled);
                ImGui::PushStyleColor(ImGuiCol_Border, theme.BorderMuted);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.RoundingControl * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, theme.BorderThickness * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * scale, 6.0f * scale));
            }

            ~ControlStyle()
            {
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(15);
            }

            ControlStyle(const ControlStyle&) = delete;
            ControlStyle& operator=(const ControlStyle&) = delete;
        };

        class DisabledScope
        {
        public:
            explicit DisabledScope(bool disabled) : disabled_(disabled)
            {
                if (disabled_)
                    ImGui::BeginDisabled();
            }

            ~DisabledScope()
            {
                if (disabled_)
                    ImGui::EndDisabled();
            }

            DisabledScope(const DisabledScope&) = delete;
            DisabledScope& operator=(const DisabledScope&) = delete;

        private:
            bool disabled_;
        };
    }

    bool Checkbox(const char* label, bool* value)
    {
        bool fallback = false;
        ControlStyle style;
        DisabledScope disabled(value == nullptr);
        return ImGui::Checkbox(SafeLabel(label, "##ivui_checkbox"), value ? value : &fallback) && value != nullptr;
    }

    bool RadioButton(const char* label, bool active)
    {
        ControlStyle style;
        return ImGui::RadioButton(SafeLabel(label, "##ivui_radio"), active);
    }

    bool RadioButton(const char* label, int* value, int buttonValue)
    {
        ControlStyle style;
        DisabledScope disabled(value == nullptr);
        if (!value)
        {
            ImGui::RadioButton(SafeLabel(label, "##ivui_radio"), false);
            return false;
        }
        return ImGui::RadioButton(SafeLabel(label, "##ivui_radio"), value, buttonValue);
    }

    void ProgressBar(const char* label, float fraction, ImVec2 size, const char* overlay)
    {
        const Theme& theme = GetTheme();
        const float scale = SafeScale(theme);
        fraction = std::isfinite(fraction) ? ImClamp(fraction, 0.0f, 1.0f) : 0.0f;
        size = NonNegativeSize(size);
        if (size.x <= 0.0f)
            size.x = ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
        if (size.y <= 0.0f)
            size.y = ImMax(theme.ControlHeight * scale * 0.55f, 1.0f);

        ImGui::PushID(SafeLabel(label, "##ivui_progress"));
        const float displayed = Approach(ImGui::GetID("##value"), fraction, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.AccentMuted);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, theme.Accent);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.Text);
        ImGui::PushStyleColor(ImGuiCol_Border, theme.BorderMuted);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.RoundingControl * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, theme.BorderThickness * scale);
        ImGui::ProgressBar(ImClamp(displayed, 0.0f, 1.0f), size, overlay);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
    }

    void Spinner(const char* label, float radius, float thickness)
    {
        if (ImGui::GetCurrentWindow()->SkipItems)
            return;

        const Theme& theme = GetTheme();
        const float scale = SafeScale(theme);
        radius = std::isfinite(radius) && radius > 0.0f ? radius : 8.0f * scale;
        thickness = std::isfinite(thickness) && thickness > 0.0f ? thickness : 2.0f * scale;
        thickness = ImMin(thickness, radius);

        ImGui::PushID(SafeLabel(label, "##ivui_spinner"));
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float extent = radius + thickness;
        ImGui::Dummy(ImVec2(extent * 2.0f, extent * 2.0f));

        const ImVec2 center(cursor.x + extent, cursor.y + extent);
        const float time = static_cast<float>(ImGui::GetTime());
        const float start = time * 5.0f;
        const float arc = IM_PI * (1.15f + 0.35f * std::sin(time * 3.0f));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PathClear();
        draw->PathArcTo(center, radius, start, start + arc, 24);
        draw->PathStroke(theme.Accent, 0, thickness);
        ImGui::PopID();
    }

    bool InputInt(const char* label, int* value, int minValue, int maxValue,
                  int step, int stepFast, ImGuiInputTextFlags flags)
    {
        if (minValue > maxValue)
            std::swap(minValue, maxValue);
        if (step < 0)
            step = step == INT_MIN ? INT_MAX : -step;
        if (stepFast < 0)
            stepFast = stepFast == INT_MIN ? INT_MAX : -stepFast;

        const int previous = value ? *value : 0;
        int fallback = ImClamp(0, minValue, maxValue);
        int* target = value ? value : &fallback;
        if (value)
            *value = ImClamp(*value, minValue, maxValue);

        ControlStyle style;
        DisabledScope disabled(value == nullptr);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool edited = ImGui::InputInt(SafeLabel(label, "##ivui_input_int"), target, step, stepFast, flags);
        if (value)
            *value = ImClamp(*value, minValue, maxValue);
        return value != nullptr && (edited || *value != previous);
    }

    bool InputFloat(const char* label, float* value, float minValue, float maxValue,
                    float step, float stepFast, const char* format, ImGuiInputTextFlags flags)
    {
        if (!std::isfinite(minValue))
            minValue = -FLT_MAX;
        if (!std::isfinite(maxValue))
            maxValue = FLT_MAX;
        if (minValue > maxValue)
            std::swap(minValue, maxValue);
        step = std::isfinite(step) ? std::fabs(step) : 0.0f;
        stepFast = std::isfinite(stepFast) ? std::fabs(stepFast) : 0.0f;

        const float previous = value ? *value : 0.0f;
        float fallback = ImClamp(0.0f, minValue, maxValue);
        float* target = value ? value : &fallback;
        if (value)
            *value = std::isnan(*value) ? fallback : ImClamp(*value, minValue, maxValue);

        ControlStyle style;
        DisabledScope disabled(value == nullptr);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool edited = ImGui::InputFloat(SafeLabel(label, "##ivui_input_float"), target,
                                              step, stepFast, format ? format : "%.3f", flags);
        if (value)
            *value = std::isnan(*value) ? fallback : ImClamp(*value, minValue, maxValue);
        return value != nullptr && (edited || *value != previous);
    }

    bool InputTextMultiline(const char* label, char* buffer, std::size_t bufferSize,
                            ImVec2 size, ImGuiInputTextFlags flags,
                            ImGuiInputTextCallback callback, void* userData)
    {
        char fallback[1] = { '\0' };
        const bool valid = buffer && bufferSize > 0;
        size = NonNegativeSize(size);
        ControlStyle style;
        DisabledScope disabled(!valid);
        ImGui::SetNextItemWidth(-FLT_MIN);
        return ImGui::InputTextMultiline(SafeLabel(label, "##ivui_multiline"),
                                         valid ? buffer : fallback, valid ? bufferSize : sizeof(fallback),
                                         size, flags, callback, userData) && valid;
    }

    bool SearchInput(const char* label, char* buffer, std::size_t bufferSize,
                     const char* hint, float width, ImGuiInputTextFlags flags,
                     ImGuiInputTextCallback callback, void* userData)
    {
        if (ImGui::GetCurrentWindow()->SkipItems)
            return false;

        const Theme& theme = GetTheme();
        const float scale = SafeScale(theme);
        char fallback[1] = { '\0' };
        const bool valid = buffer && bufferSize > 0;
        if (!std::isfinite(width) || width <= 0.0f)
            width = -FLT_MIN;

        ControlStyle style;
        DisabledScope disabled(!valid);
        ImGui::PushID(SafeLabel(label, "##ivui_search"));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(29.0f * scale, 6.0f * scale));
        ImGui::SetNextItemWidth(width);
        const bool edited = ImGui::InputTextWithHint("##field", hint ? hint : "",
                                                     valid ? buffer : fallback,
                                                     valid ? bufferSize : sizeof(fallback),
                                                     flags, callback, userData);
        const bool active = ImGui::IsItemActive();
        ImGui::PopStyleVar();

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        const ImVec2 center(itemMin.x + 14.0f * scale, (itemMin.y + itemMax.y) * 0.5f - scale);
        const float radius = 4.5f * scale;
        const float line = ImMax(1.4f * scale, 1.0f);
        const ImU32 iconColor = !valid ? theme.TextDisabled : (active ? theme.Accent : theme.TextMuted);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddCircle(center, radius, iconColor, 16, line);
        draw->AddLine(ImVec2(center.x + radius * 0.7f, center.y + radius * 0.7f),
                      ImVec2(center.x + radius * 1.45f, center.y + radius * 1.45f), iconColor, line);
        ImGui::PopID();
        return edited && valid;
    }

    bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, ImVec2 size)
    {
        size = NonNegativeSize(size);
        ControlStyle style(selected);
        return ImGui::Selectable(SafeLabel(label, "##ivui_selectable"), selected, flags, size);
    }

    bool Selectable(const char* label, bool* selected, ImGuiSelectableFlags flags, ImVec2 size)
    {
        bool fallback = false;
        size = NonNegativeSize(size);
        ControlStyle style(selected && *selected);
        DisabledScope disabled(selected == nullptr);
        return ImGui::Selectable(SafeLabel(label, "##ivui_selectable"), selected ? selected : &fallback,
                                 flags, size) && selected != nullptr;
    }
}
