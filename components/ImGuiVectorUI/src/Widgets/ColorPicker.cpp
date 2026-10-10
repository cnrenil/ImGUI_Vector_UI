#include "ImGuiVectorUI/Widgets/ColorPicker.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace IVUI
{
    namespace
    {
        constexpr float kTwoPi = IM_PI * 2.0f;

        float Clamp01(float value)
        {
            return std::isfinite(value) ? ImClamp(value, 0.0f, 1.0f) : 0.0f;
        }

        void SanitizeColor(ImVec4& color)
        {
            color.x = Clamp01(color.x);
            color.y = Clamp01(color.y);
            color.z = Clamp01(color.z);
            color.w = Clamp01(color.w);
        }

        bool SameColor(const ImVec4& lhs, const ImVec4& rhs, float epsilon = 0.0005f)
        {
            return std::fabs(lhs.x - rhs.x) <= epsilon &&
                   std::fabs(lhs.y - rhs.y) <= epsilon &&
                   std::fabs(lhs.z - rhs.z) <= epsilon &&
                   std::fabs(lhs.w - rhs.w) <= epsilon;
        }

        void ToHsv(const ImVec4& color, float& hue, float& saturation, float& value)
        {
            ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, hue, saturation, value);
        }

        ImVec4 FromHsv(float hue, float saturation, float value, float alpha)
        {
            hue = hue - std::floor(hue);
            float red = 0.0f;
            float green = 0.0f;
            float blue = 0.0f;
            ImGui::ColorConvertHSVtoRGB(hue, Clamp01(saturation), Clamp01(value), red, green, blue);
            return ImVec4(red, green, blue, Clamp01(alpha));
        }

        ImU32 BlendColor(ImU32 lhs, ImU32 rhs, float amount)
        {
            const ImVec4 a = ImGui::ColorConvertU32ToFloat4(lhs);
            const ImVec4 b = ImGui::ColorConvertU32ToFloat4(rhs);
            const float t = Clamp01(amount);
            return ImGui::ColorConvertFloat4ToU32(ImVec4(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t));
        }

        void FormatHex(const ImVec4& color, char* buffer, std::size_t capacity)
        {
            if (!buffer || capacity == 0)
                return;
            const int red = static_cast<int>(Clamp01(color.x) * 255.0f + 0.5f);
            const int green = static_cast<int>(Clamp01(color.y) * 255.0f + 0.5f);
            const int blue = static_cast<int>(Clamp01(color.z) * 255.0f + 0.5f);
            const int alpha = static_cast<int>(Clamp01(color.w) * 255.0f + 0.5f);
            std::snprintf(buffer, capacity, "%02X%02X%02X%02X", red, green, blue, alpha);
        }

        int HexDigit(char value)
        {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            if (value >= 'A' && value <= 'F')
                return value - 'A' + 10;
            return -1;
        }

        bool ParseHex(const char* text, ImVec4& color)
        {
            if (!text)
                return false;
            std::size_t offset = text[0] == '#' ? 1 : 0;
            const std::size_t length = std::strlen(text + offset);
            if (length != 6 && length != 8)
                return false;

            unsigned values[4] = {0, 0, 0, 255};
            for (std::size_t channel = 0; channel < (length / 2); ++channel)
            {
                const int high = HexDigit(text[offset + channel * 2]);
                const int low = HexDigit(text[offset + channel * 2 + 1]);
                if (high < 0 || low < 0)
                    return false;
                values[channel] = static_cast<unsigned>(high * 16 + low);
            }

            color = ImVec4(values[0] / 255.0f, values[1] / 255.0f,
                           values[2] / 255.0f, values[3] / 255.0f);
            return true;
        }

        bool ContainsColor(const std::vector<ImVec4>& colors, const ImVec4& color)
        {
            return std::find_if(colors.begin(), colors.end(), [&](const ImVec4& item) {
                       return SameColor(item, color, 0.005f);
                   }) != colors.end();
        }

        void PushHistory(ColorPickerState& state, ImVec4 color)
        {
            SanitizeColor(color);
            auto existing = std::find_if(state.history.begin(), state.history.end(),
                                         [&](const ImVec4& item) {
                                             return SameColor(item, color, 0.005f);
                                         });
            if (existing != state.history.end())
                state.history.erase(existing);
            state.history.insert(state.history.begin(), color);
            if (state.history.size() > ColorPickerHistoryCapacity)
                state.history.resize(ColorPickerHistoryCapacity);
        }

        void FlushPendingHistory(ColorPickerState& state)
        {
            if (!state.historyPending)
                return;
            PushHistory(state, state.pendingHistoryColor);
            state.historyPending = false;
            state.pendingHistoryId = 0;
        }

        void NoteColorChange(ColorPickerState& state, ImGuiID pickerId, const ImVec4& color)
        {
            if (state.historyPending && state.pendingHistoryId != pickerId)
                FlushPendingHistory(state);
            state.pendingHistoryId = pickerId;
            state.pendingHistoryColor = color;
            state.historyPending = true;
        }

        void DrawPickerLabel(const Theme& theme, const char* label)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const ImVec2 size = CalcTextScaled(label, theme.FontSizeSmall);
            AddTextScaled(draw, position, theme.TextMuted, label, theme.FontSizeSmall);
            ImGui::Dummy(ImVec2(0.0f, size.y + 3.0f * theme.Scale));
        }

        void DrawCheckerboard(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum,
                              const Theme& theme)
        {
            const float cell = 6.0f * theme.Scale;
            for (float y = minimum.y; y < maximum.y; y += cell)
                for (float x = minimum.x; x < maximum.x; x += cell)
                {
                    const int ix = static_cast<int>((x - minimum.x) / cell);
                    const int iy = static_cast<int>((y - minimum.y) / cell);
                    const ImVec2 cellMax(ImMin(x + cell, maximum.x), ImMin(y + cell, maximum.y));
                    draw->AddRectFilled(ImVec2(x, y), cellMax,
                                        ((ix + iy) & 1) ? theme.Surface1 : theme.Surface2);
                }
        }

        float HoverProgress(ImGuiID id, bool hovered)
        {
            if (GetContext() == nullptr)
                return hovered ? 1.0f : 0.0f;
            return Approach(id, hovered ? 1.0f : 0.0f, 12.0f);
        }

        bool DrawHueWheel(const Theme& theme, const char* id, float size, float& hue)
        {
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(id, ImVec2(size, size), ImGuiButtonFlags_EnableNav);
            const bool active = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const ImVec2 center(minimum.x + size * 0.5f, minimum.y + size * 0.5f);
            const float outer = ImMax(size * 0.5f - 3.0f * theme.Scale, 8.0f * theme.Scale);
            const float inner = ImMax(outer - 15.0f * theme.Scale, 2.0f * theme.Scale);
            bool changed = false;

            if (active)
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float dx = mouse.x - center.x;
                const float dy = mouse.y - center.y;
                const float radius = std::sqrt(dx * dx + dy * dy);
                if (radius >= inner && radius <= outer + 5.0f * theme.Scale)
                {
                    hue = std::atan2(dy, dx) / kTwoPi + 0.25f;
                    hue -= std::floor(hue);
                    changed = true;
                }
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();
            constexpr int segments = 48;
            for (int index = 0; index < segments; ++index)
            {
                const float a0 = -IM_PI * 0.5f + kTwoPi * index / segments;
                const float a1 = -IM_PI * 0.5f + kTwoPi * (index + 1) / segments;
                const ImVec2 outer0(center.x + std::cos(a0) * outer, center.y + std::sin(a0) * outer);
                const ImVec2 outer1(center.x + std::cos(a1) * outer, center.y + std::sin(a1) * outer);
                const ImVec2 inner1(center.x + std::cos(a1) * inner, center.y + std::sin(a1) * inner);
                const ImVec2 inner0(center.x + std::cos(a0) * inner, center.y + std::sin(a0) * inner);
                draw->AddQuadFilled(outer0, outer1, inner1, inner0,
                                    ImGui::ColorConvertFloat4ToU32(
                                        FromHsv(static_cast<float>(index) / segments, 1.0f, 1.0f, 1.0f)));
            }
            draw->AddCircle(center, outer + 1.0f * theme.Scale, theme.BorderMuted, 48,
                            theme.BorderThickness * theme.Scale);
            draw->AddCircle(center, inner, theme.Surface1, 48, theme.BorderThickness * theme.Scale);

            const float markerAngle = hue * kTwoPi - IM_PI * 0.5f;
            const float markerRadius = (outer + inner) * 0.5f;
            const ImVec2 marker(center.x + std::cos(markerAngle) * markerRadius,
                                center.y + std::sin(markerAngle) * markerRadius);
            draw->AddCircleFilled(marker, 5.0f * theme.Scale, theme.Surface0, 16);
            draw->AddCircle(marker, 4.0f * theme.Scale, theme.Text, 16,
                            ImMax(1.0f, theme.BorderThickness * theme.Scale));
            return changed;
        }

        bool DrawSaturationValue(const Theme& theme, const char* id, float size,
                                 float hue, float& saturation, float& value)
        {
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(id, ImVec2(size, size), ImGuiButtonFlags_EnableNav);
            const bool active = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            bool changed = false;
            if (active)
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                saturation = ImClamp((mouse.x - minimum.x) / size, 0.0f, 1.0f);
                value = 1.0f - ImClamp((mouse.y - minimum.y) / size, 0.0f, 1.0f);
                changed = true;
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 hueColor = ImGui::ColorConvertFloat4ToU32(FromHsv(hue, 1.0f, 1.0f, 1.0f));
            draw->AddRectFilledMultiColor(minimum, minimum + ImVec2(size, size),
                                          IM_COL32_WHITE, hueColor, IM_COL32_BLACK, IM_COL32_BLACK);
            draw->AddRect(minimum, minimum + ImVec2(size, size), theme.Border,
                          theme.RoundingControl * theme.Scale, 0,
                          theme.BorderThickness * theme.Scale);

            const ImVec2 marker(minimum.x + saturation * size,
                                minimum.y + (1.0f - value) * size);
            draw->AddCircleFilled(marker, 5.0f * theme.Scale, theme.Surface0, 16);
            draw->AddCircle(marker, 4.0f * theme.Scale, theme.Text, 16,
                            ImMax(1.0f, theme.BorderThickness * theme.Scale));
            return changed;
        }

        bool DrawColorTrack(const Theme& theme, const char* label, const char* id,
                            ImVec2 size, bool alpha, float hue, float saturation,
                            float& value, ImVec4& color)
        {
            DrawPickerLabel(theme, label);
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
            const bool active = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            bool changed = false;
            if (active)
            {
                value = ImClamp((ImGui::GetMousePos().x - minimum.x) / ImMax(size.x, 1.0f), 0.0f, 1.0f);
                changed = true;
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();
            if (alpha)
                DrawCheckerboard(draw, minimum, minimum + size, theme);

            const ImVec4 left = alpha
                ? ImVec4(color.x, color.y, color.z, 0.0f)
                : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            const ImVec4 right = alpha
                ? ImVec4(color.x, color.y, color.z, 1.0f)
                : FromHsv(hue, saturation, 1.0f, 1.0f);
            draw->AddRectFilledMultiColor(minimum, minimum + size,
                                          ImGui::ColorConvertFloat4ToU32(left),
                                          ImGui::ColorConvertFloat4ToU32(right),
                                          ImGui::ColorConvertFloat4ToU32(right),
                                          ImGui::ColorConvertFloat4ToU32(left));
            draw->AddRect(minimum, minimum + size, theme.BorderMuted,
                          theme.RoundingControl * theme.Scale, 0,
                          theme.BorderThickness * theme.Scale);

            const float markerX = minimum.x + value * size.x;
            draw->AddLine(ImVec2(markerX, minimum.y - 2.0f * theme.Scale),
                          ImVec2(markerX, minimum.y + size.y + 2.0f * theme.Scale),
                          theme.Text, ImMax(1.0f, theme.BorderThickness * theme.Scale));
            if (changed)
                color = alpha ? ImVec4(color.x, color.y, color.z, value)
                              : FromHsv(hue, saturation, value, color.w);
            return changed;
        }

        class ColorPickerControlStyle
        {
        public:
            explicit ColorPickerControlStyle(const Theme& theme)
            {
                const float scale = theme.Scale;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme.AccentMuted);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, theme.Accent);
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, theme.Accent);
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, theme.AccentActive);
                ImGui::PushStyleColor(ImGuiCol_Button, theme.Surface2);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.AccentHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.AccentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, theme.Text);
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, theme.TextDisabled);
                ImGui::PushStyleColor(ImGuiCol_Border, theme.BorderMuted);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.RoundingControl * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, theme.BorderThickness * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f * scale, 5.0f * scale));
            }

            ~ColorPickerControlStyle()
            {
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(12);
            }

            ColorPickerControlStyle(const ColorPickerControlStyle&) = delete;
            ColorPickerControlStyle& operator=(const ColorPickerControlStyle&) = delete;
        };

        bool DrawIntField(const Theme& theme, const char* label, float width, int& value,
                          int minimum, int maximum)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const ImVec2 labelSize = CalcTextScaled(label, theme.FontSizeSmall);
            AddTextScaled(draw, position, theme.TextMuted, label, theme.FontSizeSmall);
            ImGui::Dummy(ImVec2(width, labelSize.y + 3.0f * theme.Scale));
            ImGui::SetNextItemWidth(width);
            const bool edited = ImGui::InputInt("##value", &value, 1, 10);
            value = ImClamp(value, minimum, maximum);
            return edited;
        }

        bool DrawFloatField(const Theme& theme, const char* label, float width, float& value,
                            float minimum, float maximum, const char* format)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const ImVec2 labelSize = CalcTextScaled(label, theme.FontSizeSmall);
            AddTextScaled(draw, position, theme.TextMuted, label, theme.FontSizeSmall);
            ImGui::Dummy(ImVec2(width, labelSize.y + 3.0f * theme.Scale));
            ImGui::SetNextItemWidth(width);
            const bool edited = ImGui::InputFloat("##value", &value, 0.0f, 0.0f, format);
            value = std::isfinite(value) ? ImClamp(value, minimum, maximum) : minimum;
            return edited;
        }

        bool DrawRgbRow(const Theme& theme, int rgb[3])
        {
            const float spacing = theme.ItemSpacing * theme.Scale;
            const float width = ImMax((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f,
                                     36.0f * theme.Scale);
            const char* labels[] = {"R", "G", "B"};
            bool edited = false;
            ImGui::PushID("rgb");
            for (int index = 0; index < 3; ++index)
            {
                if (index != 0)
                    ImGui::SameLine(0.0f, spacing);
                ImGui::PushID(index);
                edited = DrawIntField(theme, labels[index], width, rgb[index], 0, 255) || edited;
                ImGui::PopID();
            }
            ImGui::PopID();
            return edited;
        }

        bool DrawHsvRow(const Theme& theme, float hsv[3])
        {
            const float spacing = theme.ItemSpacing * theme.Scale;
            const float width = ImMax((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f,
                                     36.0f * theme.Scale);
            const char* labels[] = {"H", "S", "V"};
            bool edited = false;
            ImGui::PushID("hsv");
            for (int index = 0; index < 3; ++index)
            {
                if (index != 0)
                    ImGui::SameLine(0.0f, spacing);
                ImGui::PushID(index);
                edited = DrawFloatField(theme, labels[index], width, hsv[index],
                                        index == 0 ? 0.0f : 0.0f,
                                        index == 0 ? 360.0f : 100.0f,
                                        "%.1f") || edited;
                ImGui::PopID();
            }
            ImGui::PopID();
            return edited;
        }

        struct HexRowResult
        {
            bool changed = false;
            bool favoriteClicked = false;
            bool eyedropperClicked = false;
        };

        bool DrawIconButton(const Theme& theme, const char* id, ImVec2 size, IconType icon,
                            bool selected, bool enabled, const char* tooltip)
        {
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
            const bool hovered = enabled && ImGui::IsItemHovered();
            const float hover = HoverProgress(ImGui::GetItemID(), hovered);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 fill = selected
                ? theme.AccentMuted
                : BlendColor(theme.Surface2, theme.AccentMuted, hover);
            const ImU32 iconColor = !enabled ? theme.TextDisabled :
                (selected || hovered ? theme.Accent : theme.TextMuted);
            draw->AddRectFilled(minimum, minimum + size, fill,
                                theme.RoundingControl * theme.Scale);
            draw->AddRect(minimum, minimum + size,
                          selected || hovered ? theme.Accent : theme.BorderMuted,
                          theme.RoundingControl * theme.Scale, 0,
                          theme.BorderThickness * theme.Scale);
            if (ImGui::IsItemFocused())
                draw->AddRect(minimum - ImVec2(2.0f * theme.Scale, 2.0f * theme.Scale),
                              minimum + size + ImVec2(2.0f * theme.Scale, 2.0f * theme.Scale),
                              theme.Accent, theme.RoundingControl * theme.Scale + 2.0f * theme.Scale,
                              0, ImMax(1.0f, theme.BorderThickness * theme.Scale));
            Icon(draw, icon, minimum + size * 0.5f, 14.0f * theme.Scale,
                 iconColor, ImMax(1.0f, 1.5f * theme.Scale));
            if (hovered && tooltip)
                ImGui::SetItemTooltip("%s", tooltip);
            return enabled && clicked;
        }

        bool DrawColorSwatch(const Theme& theme, const char* id, ImVec4 color,
                             ImVec2 size, bool selected)
        {
            const ImVec2 minimum = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
            const bool hovered = ImGui::IsItemHovered();
            const float hover = HoverProgress(ImGui::GetItemID(), hovered);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            DrawCheckerboard(draw, minimum, minimum + size, theme);
            draw->AddRectFilled(minimum, minimum + size,
                                ImGui::ColorConvertFloat4ToU32(color),
                                theme.RoundingControl * theme.Scale);
            draw->AddRect(minimum, minimum + size,
                          selected ? theme.Accent :
                              BlendColor(theme.BorderMuted, theme.AccentHover, hover),
                          theme.RoundingControl * theme.Scale, 0,
                          (selected ? 2.0f : theme.BorderThickness) * theme.Scale);
            return clicked;
        }

        HexRowResult DrawHexRow(const Theme& theme, ColorPickerState* state, ImVec4& color,
                                const ColorPickerOptions& options)
        {
            HexRowResult result;
            DrawPickerLabel(theme, "HEX");
            const float scale = theme.Scale;
            const float spacing = theme.ItemSpacing * scale;
            const float buttonSize = ImMax(26.0f * scale, theme.ControlHeight * 0.72f * scale);
            const int buttonCount = (options.showFavorites ? 1 : 0) +
                                    (options.showEyedropper ? 1 : 0);
            const float reserved = buttonCount > 0
                ? buttonCount * buttonSize + (buttonCount - 1) * spacing + spacing
                : 0.0f;
            const float inputWidth = ImMax(ImGui::GetContentRegionAvail().x - reserved,
                                           80.0f * scale);

            char fallback[9] = {};
            char* buffer = fallback;
            if (state)
                buffer = state->hexBuffer.data();

            ImGui::PushID("hex");
            const ImGuiID fieldId = ImGui::GetID("##value");
            if (state)
            {
                if (state->hexBufferId != fieldId)
                {
                    state->hexBufferId = fieldId;
                    FormatHex(color, buffer, 9);
                }
                else if (ImGui::GetActiveID() != fieldId)
                {
                    FormatHex(color, buffer, 9);
                }
            }
            else
            {
                FormatHex(color, buffer, sizeof(fallback));
            }

            ImGui::SetNextItemWidth(inputWidth);
            const bool edited = ImGui::InputText("##value", buffer, 9,
                                                 ImGuiInputTextFlags_CharsHexadecimal |
                                                     ImGuiInputTextFlags_CharsUppercase);
            if (edited)
            {
                ImVec4 parsed;
                if (ParseHex(buffer, parsed))
                {
                    color = parsed;
                    result.changed = true;
                }
            }

            const ImVec4 safeColor = color;
            const bool favorite = state && ContainsColor(state->favorites, safeColor);
            if (options.showFavorites)
            {
                ImGui::SameLine(0.0f, spacing);
                const bool enabled = state &&
                    (favorite || state->favorites.size() < ColorPickerFavoriteCapacity);
                result.favoriteClicked = DrawIconButton(
                    theme, "favorite", ImVec2(buttonSize, buttonSize), IconType::Star,
                    favorite, enabled,
                    favorite ? "Remove from favorites" : "Add to favorites");
            }
            if (options.showEyedropper)
            {
                ImGui::SameLine(0.0f, spacing);
                result.eyedropperClicked = DrawIconButton(
                    theme, "eyedropper", ImVec2(buttonSize, buttonSize), IconType::Crosshair,
                    false, state && state->eyedropper != nullptr,
                    "Pick color from screen");
            }
            ImGui::PopID();
            return result;
        }

        bool DrawPaletteRow(const Theme& theme, const char* label,
                            const std::vector<ImVec4>* colors, ImVec4& color)
        {
            if (!colors)
                return false;
            DrawPickerLabel(theme, label);
            if (colors->empty())
            {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const ImVec2 position = ImGui::GetCursorScreenPos();
                const ImVec2 size = CalcTextScaled("None yet", theme.FontSizeSmall);
                AddTextScaled(draw, position, theme.TextDisabled, "None yet", theme.FontSizeSmall);
                ImGui::Dummy(ImVec2(0.0f, size.y + 2.0f * theme.Scale));
                return false;
            }

            const float spacing = theme.ItemSpacing * theme.Scale;
            const ImVec2 swatchSize(28.0f * theme.Scale, 28.0f * theme.Scale);
            bool changed = false;
            ImGui::PushID(label);
            for (std::size_t index = 0; index < colors->size(); ++index)
            {
                if (index != 0)
                    ImGui::SameLine(0.0f, spacing);
                ImGui::PushID(static_cast<int>(index));
                const ImVec4 item = (*colors)[index];
                if (DrawColorSwatch(theme, "swatch", item, swatchSize, SameColor(item, color)))
                {
                    color = item;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::PopID();
            return changed;
        }

        bool DrawCaptionedSwatch(const Theme& theme, const char* id, const char* caption,
                                 ImVec4 color, ImVec4& target)
        {
            ImGui::BeginGroup();
            const ImVec2 swatchSize(32.0f * theme.Scale, 32.0f * theme.Scale);
            const ImVec2 textSize = CalcTextScaled(caption, theme.FontSizeSmall);
            const float groupWidth = ImMax(swatchSize.x, textSize.x);
            const ImVec2 groupStart = ImGui::GetCursorScreenPos();
            if (groupWidth > swatchSize.x)
                ImGui::SetCursorScreenPos(groupStart + ImVec2((groupWidth - swatchSize.x) * 0.5f, 0.0f));
            const bool clicked = DrawColorSwatch(theme, id, color, swatchSize, SameColor(color, target));
            const ImVec2 textPosition = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(groupStart.x, textPosition.y));
            AddTextScaled(ImGui::GetWindowDrawList(),
                          ImVec2(groupStart.x + (groupWidth - textSize.x) * 0.5f,
                                 textPosition.y),
                          theme.TextMuted, caption, theme.FontSizeSmall);
            ImGui::Dummy(ImVec2(groupWidth, textSize.y + 2.0f * theme.Scale));
            ImGui::EndGroup();
            if (clicked)
                target = color;
            return clicked;
        }

        bool DrawRelatedRow(const Theme& theme, ImVec4& color)
        {
            float hue = 0.0f;
            float saturation = 0.0f;
            float value = 0.0f;
            ToHsv(color, hue, saturation, value);
            const ImVec4 analogousLeft = FromHsv(hue - 1.0f / 12.0f, saturation, value, color.w);
            const ImVec4 complementary = FromHsv(hue + 0.5f, saturation, value, color.w);
            const ImVec4 analogousRight = FromHsv(hue + 1.0f / 12.0f, saturation, value, color.w);

            DrawPickerLabel(theme, "Related");
            const float spacing = theme.ItemSpacing * theme.Scale;
            bool changed = false;
            ImGui::PushID("related");
            changed = DrawCaptionedSwatch(theme, "analogous_left", "Analogous -",
                                          analogousLeft, color) || changed;
            ImGui::SameLine(0.0f, spacing);
            changed = DrawCaptionedSwatch(theme, "complementary", "Complement",
                                          complementary, color) || changed;
            ImGui::SameLine(0.0f, spacing);
            changed = DrawCaptionedSwatch(theme, "analogous_right", "Analogous +",
                                          analogousRight, color) || changed;
            ImGui::PopID();
            return changed;
        }
    }

    ColorPickerState* GetColorPickerState() noexcept
    {
        Context* context = GetContext();
        return context ? &context->colorPicker : nullptr;
    }

    void SetColorPickerEyedropper(ColorPickerEyedropperCallback callback, void* userData) noexcept
    {
        if (ColorPickerState* state = GetColorPickerState())
        {
            state->eyedropper = callback;
            state->eyedropperUserData = userData;
        }
    }

    bool AddColorPickerFavorite(const ImVec4& color)
    {
        ColorPickerState* state = GetColorPickerState();
        if (!state)
            return false;
        ImVec4 safeColor = color;
        SanitizeColor(safeColor);
        if (ContainsColor(state->favorites, safeColor) ||
            state->favorites.size() >= ColorPickerFavoriteCapacity)
            return false;
        state->favorites.push_back(safeColor);
        return true;
    }

    bool RemoveColorPickerFavorite(const ImVec4& color)
    {
        ColorPickerState* state = GetColorPickerState();
        if (!state)
            return false;
        const auto found = std::find_if(state->favorites.begin(), state->favorites.end(),
                                        [&](const ImVec4& item) {
                                            return SameColor(item, color, 0.005f);
                                        });
        if (found == state->favorites.end())
            return false;
        state->favorites.erase(found);
        return true;
    }

    void ClearColorPickerHistory() noexcept
    {
        if (ColorPickerState* state = GetColorPickerState())
        {
            state->history.clear();
            state->historyPending = false;
            state->pendingHistoryId = 0;
        }
    }

    void ClearColorPickerFavorites() noexcept
    {
        if (ColorPickerState* state = GetColorPickerState())
            state->favorites.clear();
    }

    bool ColorPicker(const char* label, ImVec4* color)
    {
        return ColorPicker(label, color, {});
    }

    bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options)
    {
        if (!color)
            return false;

        const Theme& theme = GetTheme();
        const float scale = std::isfinite(theme.Scale) && theme.Scale > 0.0f ? theme.Scale : 1.0f;
        const char* pickerLabel = label && label[0] != '\0' ? label : "Color";
        const ImVec4 before = *color;
        SanitizeColor(*color);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float height = theme.ControlHeight * scale * 0.62f;
        const float swatchWidth = 46.0f * scale;
        const float rightInset = 12.0f * scale;
        const ImVec2 labelSize = CalcTextScaled(pickerLabel, theme.FontSizeLabel);
        AddTextScaled(draw, ImVec2(cursor.x, cursor.y + (height - labelSize.y) * 0.5f),
                      theme.Text, pickerLabel, theme.FontSizeLabel);

        const ImVec2 swatchMinimum(cursor.x + ImMax(available.x - rightInset - swatchWidth, 0.0f),
                                   cursor.y);
        const ImVec2 swatchSize(swatchWidth, height);

        ImGui::PushID(pickerLabel);
        const ImGuiID pickerId = ImGui::GetID("picker");
        ColorPickerState* state = GetColorPickerState();
        if (state && state->historyPending && state->pendingHistoryId != pickerId)
            FlushPendingHistory(*state);
        if (state && state->historyPending && state->pendingHistoryId == pickerId &&
            !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            FlushPendingHistory(*state);

        ImGui::SetCursorScreenPos(swatchMinimum);
        const bool swatchClicked = ImGui::InvisibleButton("swatch", swatchSize,
                                                           ImGuiButtonFlags_EnableNav);
        const bool swatchHovered = ImGui::IsItemHovered();
            ElementOptions swatchElement;
        swatchElement.role = ElementRole::ColorPicker;
        if (options.element)
            swatchElement = *options.element;
        {
            ElementScope scope = BeginElement(draw, ImGui::GetItemID(), true, swatchElement);
            DrawCheckerboard(draw, swatchMinimum, swatchMinimum + swatchSize, theme);
            draw->AddRectFilled(swatchMinimum, swatchMinimum + swatchSize,
                                ImGui::ColorConvertFloat4ToU32(*color),
                                theme.RoundingControl * scale);
            draw->AddRect(swatchMinimum, swatchMinimum + swatchSize,
                          swatchHovered ? theme.Accent : theme.BorderMuted,
                          theme.RoundingControl * scale, 0,
                          (swatchHovered ? 1.5f : theme.BorderThickness) * scale);
            if (ImGui::IsItemFocused())
                draw->AddRect(swatchMinimum - ImVec2(2.0f * scale, 2.0f * scale),
                              swatchMinimum + swatchSize + ImVec2(2.0f * scale, 2.0f * scale),
                              theme.Accent, theme.RoundingControl * scale + 2.0f * scale,
                              0, ImMax(1.0f, theme.BorderThickness * scale));
            scope.End();
        }
        if (swatchClicked)
            ImGui::OpenPopup("popup");

        bool changed = !SameColor(before, *color);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, theme.Surface1);
        ImGui::PushStyleColor(ImGuiCol_Border, theme.Border);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.Text);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme.RoundingPanel * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, theme.BorderThickness * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(theme.PanelPadding.x * scale, theme.PanelPadding.y * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(theme.ItemSpacing * scale, 6.0f * scale));
        ImGui::SetNextWindowSize(ImVec2(540.0f * scale, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f * scale, 0.0f),
                                             ImVec2(720.0f * scale, FLT_MAX));

        if (ImGui::BeginPopup("popup"))
        {
            ElementOptions popupElement;
            popupElement.role = ElementRole::ColorPicker;
            if (options.element)
                popupElement = *options.element;
            ElementScope popupScope = BeginElement(draw, ImGui::GetID("popup_element"), true,
                                                   popupElement);

            float hue = 0.0f;
            float saturation = 0.0f;
            float value = 0.0f;
            ToHsv(*color, hue, saturation, value);

            const float gap = theme.ItemSpacing * scale;
            const float popupWidth = ImGui::GetContentRegionAvail().x;
            const float svSize = ImMax(128.0f * scale,
                                       ImMin(220.0f * scale, (popupWidth - gap) * 0.5f));
            const float rightWidth = ImMax(140.0f * scale, popupWidth - svSize - gap);
            const float wheelSize = ImMin(176.0f * scale, rightWidth);

            ImGui::BeginGroup();
            DrawPickerLabel(theme, "Saturation / Value");
            if (DrawSaturationValue(theme, "sv", svSize, hue, saturation, value))
            {
                *color = FromHsv(hue, saturation, value, color->w);
                changed = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, gap);
            ImGui::BeginGroup();
            DrawPickerLabel(theme, "Hue");
            if (DrawHueWheel(theme, "hue", wheelSize, hue))
            {
                *color = FromHsv(hue, saturation, value, color->w);
                changed = true;
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f * scale));
            const ImVec2 trackSize(rightWidth, ImMax(10.0f * scale, 12.0f * scale));
            if (DrawColorTrack(theme, "Brightness", "brightness", trackSize, false,
                               hue, saturation, value, *color))
                changed = true;
            if (options.showAlpha)
            {
                float alpha = color->w;
                if (DrawColorTrack(theme, "Alpha", "alpha", trackSize, true,
                                   hue, saturation, alpha, *color))
                    changed = true;
            }
            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0.0f, 4.0f * scale));
            {
                ColorPickerControlStyle controls(theme);
                DrawPickerLabel(theme, "RGB");
                int rgb[3] = {
                    static_cast<int>(Clamp01(color->x) * 255.0f + 0.5f),
                    static_cast<int>(Clamp01(color->y) * 255.0f + 0.5f),
                    static_cast<int>(Clamp01(color->z) * 255.0f + 0.5f)};
                const bool rgbEdited = DrawRgbRow(theme, rgb);
                if (rgbEdited)
                {
                    color->x = rgb[0] / 255.0f;
                    color->y = rgb[1] / 255.0f;
                    color->z = rgb[2] / 255.0f;
                    changed = true;
                }

                ToHsv(*color, hue, saturation, value);
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                DrawPickerLabel(theme, "HSV");
                float hsv[3] = {hue * 360.0f, saturation * 100.0f, value * 100.0f};
                const bool hsvEdited = DrawHsvRow(theme, hsv);
                if (hsvEdited)
                {
                    hue = hsv[0] / 360.0f;
                    saturation = hsv[1] / 100.0f;
                    value = hsv[2] / 100.0f;
                    *color = FromHsv(hue, saturation, value, color->w);
                    changed = true;
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                const HexRowResult hex = DrawHexRow(theme, state, *color, options);
                changed = hex.changed || changed;
                if (hex.favoriteClicked && state)
                {
                    if (ContainsColor(state->favorites, *color))
                        RemoveColorPickerFavorite(*color);
                    else
                        AddColorPickerFavorite(*color);
                }
                if (hex.eyedropperClicked && state && state->eyedropper)
                {
                    ImVec4 sampled = *color;
                    if (state->eyedropper(ImGui::GetMousePos(), sampled,
                                          state->eyedropperUserData))
                    {
                        SanitizeColor(sampled);
                        *color = sampled;
                        changed = true;
                    }
                }
            }

            if (options.showHistory && state)
            {
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                changed = DrawPaletteRow(theme, "History", &state->history, *color) || changed;
            }
            if (options.showFavorites && state)
            {
                ImGui::Dummy(ImVec2(0.0f, 3.0f * scale));
                changed = DrawPaletteRow(theme, "Favorites", &state->favorites, *color) || changed;
            }
            if (options.showRelated)
            {
                ImGui::Dummy(ImVec2(0.0f, 3.0f * scale));
                changed = DrawRelatedRow(theme, *color) || changed;
            }

            popupScope.End();
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        SanitizeColor(*color);
        changed = !SameColor(before, *color) || changed;
        if (state && changed)
            NoteColorChange(*state, pickerId, *color);
        if (state && state->historyPending && state->pendingHistoryId == pickerId &&
            !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            FlushPendingHistory(*state);
        return changed;
    }
}
