#include "ImGuiVectorUI/Widgets/Notification.h"

#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include "imgui_internal.h"

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace IVUI
{
    namespace
    {
        struct ResolvedNotification
        {
            NotificationType type = NotificationType::Info;
            NotificationStyle style = NotificationStyle::Card;
            NotificationPlacement placement = NotificationPlacement::BottomRight;
            ImVec2 position{};
            ImVec2 offset{};
            ImVec2 viewportPadding{};
            float width = 0.0f;
            float maxWidth = 0.0f;
            float spacing = 0.0f;
            float duration = 0.0f;
            bool pauseOnHover = true;
            bool dismissOnClick = true;
            bool showProgress = true;
            bool clampToViewport = true;
        };

        struct PendingAction
        {
            NotificationHandle handle{};
            void (*callback)(NotificationHandle, void*) = nullptr;
            void* userData = nullptr;
        };

        NotificationState* FindNotification(Context& context, NotificationHandle handle)
        {
            for (NotificationState& state : context.notifications)
                if (state.handle == handle)
                    return &state;
            return nullptr;
        }

        void StoreActionLabel(NotificationState& state)
        {
            if (!state.options.action)
            {
                state.actionLabel.clear();
                return;
            }
            state.actionLabel = state.options.action->label
                ? state.options.action->label : "";
            state.options.action->label = nullptr;
        }

        void ApplyOptions(NotificationState& state, const NotificationOptions& options)
        {
            if (options.type) state.options.type = options.type;
            if (options.style) state.options.style = options.style;
            if (options.placement) state.options.placement = options.placement;
            if (options.position) state.options.position = options.position;
            if (options.offset) state.options.offset = options.offset;
            if (options.viewportPadding) state.options.viewportPadding = options.viewportPadding;
            if (options.width) state.options.width = options.width;
            if (options.maxWidth) state.options.maxWidth = options.maxWidth;
            if (options.spacing) state.options.spacing = options.spacing;
            if (options.duration) state.options.duration = options.duration;
            if (options.pauseOnHover) state.options.pauseOnHover = options.pauseOnHover;
            if (options.dismissOnClick) state.options.dismissOnClick = options.dismissOnClick;
            if (options.showProgress) state.options.showProgress = options.showProgress;
            if (options.clampToViewport) state.options.clampToViewport = options.clampToViewport;
            if (options.progress) state.options.progress = options.progress;
            if (options.dedupeKey)
            {
                state.options.dedupeKey = options.dedupeKey;
                state.dedupeKey = *options.dedupeKey;
            }
            if (options.effects) state.options.effects = options.effects;
            if (options.motion) state.options.motion = options.motion;
            if (options.action)
            {
                state.options.action = options.action;
                StoreActionLabel(state);
            }
        }

        ImU32 TypeColor(NotificationType type, const Theme& theme)
        {
            switch (type)
            {
            case NotificationType::Success: return theme.Success;
            case NotificationType::Warning: return theme.Warning;
            case NotificationType::Error:   return theme.Error;
            case NotificationType::Info:
            default:                        return theme.Info;
            }
        }

        const char* TypeGlyph(NotificationType type)
        {
            switch (type)
            {
            case NotificationType::Success: return "+";
            case NotificationType::Warning: return "!";
            case NotificationType::Error:   return "x";
            case NotificationType::Info:
            default:                        return "i";
            }
        }

        ResolvedNotification Resolve(const NotificationState& state, const Theme& theme)
        {
            const NotificationTheme& defaults = theme.Notifications;
            const NotificationOptions& options = state.options;
            ResolvedNotification result;
            result.type = options.type.value_or(NotificationType::Info);
            result.style = options.style.value_or(defaults.style);
            result.placement = options.placement.value_or(defaults.placement);
            result.position = options.position.value_or(ImVec2{});
            result.offset = options.offset.value_or(defaults.offset) * theme.Scale;
            result.viewportPadding = options.viewportPadding.value_or(defaults.viewportPadding) * theme.Scale;
            result.width = options.width.value_or(defaults.width) * theme.Scale;
            result.maxWidth = options.maxWidth.value_or(defaults.maxWidth) * theme.Scale;
            result.spacing = options.spacing.value_or(defaults.spacing) * theme.Scale;
            result.duration = options.duration.value_or(defaults.duration);
            result.pauseOnHover = options.pauseOnHover.value_or(defaults.pauseOnHover);
            result.dismissOnClick = options.dismissOnClick.value_or(defaults.dismissOnClick);
            result.showProgress = options.showProgress.value_or(defaults.showProgress);
            result.clampToViewport = options.clampToViewport.value_or(defaults.clampToViewport);
            return result;
        }

        int PlacementIndex(NotificationPlacement placement)
        {
            switch (placement)
            {
            case NotificationPlacement::TopLeft:      return 0;
            case NotificationPlacement::TopCenter:    return 1;
            case NotificationPlacement::TopRight:     return 2;
            case NotificationPlacement::BottomLeft:   return 3;
            case NotificationPlacement::BottomCenter: return 4;
            case NotificationPlacement::BottomRight:  return 5;
            case NotificationPlacement::Custom:       return -1;
            }
            return -1;
        }

        bool IsTop(NotificationPlacement placement)
        {
            return placement == NotificationPlacement::TopLeft ||
                   placement == NotificationPlacement::TopCenter ||
                   placement == NotificationPlacement::TopRight;
        }

        ImVec2 Place(const ResolvedNotification& resolved, const ImGuiViewport& viewport,
                     ImVec2 size, float stackOffset)
        {
            ImVec2 result;
            if (resolved.placement == NotificationPlacement::Custom)
                result = resolved.position + resolved.offset;
            else
            {
                const ImVec2 padding = resolved.viewportPadding;
                float x = viewport.Pos.x + padding.x;
                if (resolved.placement == NotificationPlacement::TopCenter ||
                    resolved.placement == NotificationPlacement::BottomCenter)
                    x = viewport.Pos.x + (viewport.Size.x - size.x) * 0.5f;
                else if (resolved.placement == NotificationPlacement::TopRight ||
                         resolved.placement == NotificationPlacement::BottomRight)
                    x = viewport.Pos.x + viewport.Size.x - padding.x - size.x;

                const float y = IsTop(resolved.placement)
                    ? viewport.Pos.y + padding.y + stackOffset
                    : viewport.Pos.y + viewport.Size.y - padding.y - stackOffset - size.y;
                result = ImVec2(x, y) + resolved.offset;
            }

            if (resolved.clampToViewport)
            {
                const float maxX = ImMax(viewport.Pos.x, viewport.Pos.x + viewport.Size.x - size.x);
                const float maxY = ImMax(viewport.Pos.y, viewport.Pos.y + viewport.Size.y - size.y);
                result.x = ImClamp(result.x, viewport.Pos.x, maxX);
                result.y = ImClamp(result.y, viewport.Pos.y, maxY);
            }
            return result;
        }

        ImVec2 MeasureWrapped(ImFont* font, float fontSize, const std::string& text, float width)
        {
            if (text.empty())
                return {};
            return font->CalcTextSizeA(fontSize, width, width, text.data(), text.data() + text.size());
        }

        void AddWrapped(ImDrawList* draw, ImFont* font, float fontSize, ImVec2 position,
                        ImU32 color, const std::string& text, float width)
        {
            if (!text.empty())
                draw->AddText(font, fontSize, position, color, text.data(),
                              text.data() + text.size(), width);
        }
    }

    NotificationHandle Notify(const char* title, const char* body,
                              const NotificationOptions& options)
    {
        Context* context = GetContext();
        if (!context)
            return {};

        const Theme& theme = GetTheme();
        if (theme.Notifications.deduplicate && options.dedupeKey && !options.dedupeKey->empty())
            for (NotificationState& state : context->notifications)
                if (state.dedupeKey == *options.dedupeKey)
                {
                    state.title = title ? title : "";
                    state.body = body ? body : "";
                    ApplyOptions(state, options);
                    state.age = 0.0f;
                    state.alive = true;
                    return state.handle;
                }

        const std::size_t queueLimit = theme.Notifications.queueLimit;
        if (queueLimit > 0)
            while (context->notifications.size() >= queueLimit)
                context->notifications.erase(context->notifications.begin());

        NotificationState state;
        state.handle.value = context->nextNotificationId++;
        if (state.handle.value == 0)
            state.handle.value = context->nextNotificationId++;
        state.title = title ? title : "";
        state.body = body ? body : "";
        state.options = options;
        state.dedupeKey = options.dedupeKey.value_or(std::string{});
        StoreActionLabel(state);
        context->notifications.push_back(std::move(state));
        return context->notifications.back().handle;
    }

    bool UpdateNotification(NotificationHandle handle, const NotificationUpdate& update)
    {
        Context* context = GetContext();
        if (!context)
            return false;
        NotificationState* state = FindNotification(*context, handle);
        if (!state)
            return false;
        if (update.title) state->title = *update.title;
        if (update.body) state->body = *update.body;
        ApplyOptions(*state, update.options);
        state->age = 0.0f;
        state->alive = true;
        return true;
    }

    bool CloseNotification(NotificationHandle handle)
    {
        if (Context* context = GetContext())
            if (NotificationState* state = FindNotification(*context, handle))
            {
                state->alive = false;
                return true;
            }
        return false;
    }

    bool SetNotificationProgress(NotificationHandle handle, float progress)
    {
        if (Context* context = GetContext())
            if (NotificationState* state = FindNotification(*context, handle))
            {
                state->options.progress = ImClamp(progress, 0.0f, 1.0f);
                return true;
            }
        return false;
    }

    void ClearNotifications()
    {
        if (Context* context = GetContext())
            context->notifications.clear();
    }

    void RenderNotifications()
    {
        Context* context = GetContext();
        if (!context || context->notifications.empty())
            return;

        const Theme& theme = GetTheme();
        const float scale = theme.Scale;
        const float dt = ImClamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImDrawList* draw = ImGui::GetForegroundDrawList(viewport);
        std::array<float, 6> stackOffsets{};
        std::vector<NotificationHandle> finished;
        std::vector<PendingAction> pendingActions;

        for (auto iterator = context->notifications.rbegin();
             iterator != context->notifications.rend(); ++iterator)
        {
            NotificationState& state = *iterator;
            const ResolvedNotification resolved = Resolve(state, theme);
            const bool island = resolved.style == NotificationStyle::DynamicIsland;
            const bool compact = resolved.style == NotificationStyle::Compact;
            const bool banner = resolved.style == NotificationStyle::Banner;
            const bool glass = resolved.style == NotificationStyle::Glass;
            const float padX = (compact ? 10.0f : island ? 16.0f : banner ? 16.0f : 14.0f) * scale;
            const float padY = (compact ? 7.0f : island ? 10.0f : banner ? 12.0f : 11.0f) * scale;
            const float iconRadius = (compact ? 8.0f : 11.0f) * scale;
            const float iconGap = (compact ? 8.0f : 10.0f) * scale;
            const float titleSize = (compact ? theme.FontSizeSmall : theme.FontSizeLabel) * scale;
            const float bodySize = theme.FontSizeSmall * scale;
            ImFont* titleFont = GetTitleFont();
            ImFont* bodyFont = GetRegularFont();
            const bool hasBody = !state.body.empty();
            const bool hasAction = state.options.action &&
                                   state.options.action->callback &&
                                   !state.actionLabel.empty();
            const float availableWidth = ImMax(1.0f,
                viewport->Size.x - resolved.viewportPadding.x * 2.0f);
            const float maxWidth = resolved.maxWidth > 0.0f
                ? ImMin(resolved.maxWidth, availableWidth) : availableWidth;
            const float decorationWidth = padX * 2.0f + iconRadius * 2.0f + iconGap +
                                          (island || compact ? 0.0f : 4.0f * scale);
            const ImVec2 naturalTitle = titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f,
                state.title.data(), state.title.data() + state.title.size());
            const ImVec2 naturalBody = hasBody
                ? bodyFont->CalcTextSizeA(bodySize, FLT_MAX, 0.0f,
                    state.body.data(), state.body.data() + state.body.size()) : ImVec2{};
            const ImVec2 naturalAction = hasAction
                ? titleFont->CalcTextSizeA(bodySize, FLT_MAX, 0.0f,
                    state.actionLabel.data(), state.actionLabel.data() + state.actionLabel.size()) : ImVec2{};
            float width = resolved.width > 0.0f
                ? resolved.width
                : (banner ? maxWidth : decorationWidth + ImMax(naturalTitle.x,
                    ImMax(naturalBody.x, naturalAction.x)));
            width = ImClamp(width, ImMin(160.0f * scale, maxWidth), maxWidth);
            const float textWidth = ImMax(80.0f * scale, width - decorationWidth);
            const ImVec2 titleExtent = MeasureWrapped(titleFont, titleSize, state.title, textWidth);
            const ImVec2 bodyExtent = MeasureWrapped(bodyFont, bodySize, state.body, textWidth);
            const float lineGap = 4.0f * scale;
            const float actionGap = hasAction ? 7.0f * scale : 0.0f;
            const float actionHeight = hasAction ? naturalAction.y : 0.0f;
            const float textHeight = titleExtent.y + (hasBody ? lineGap + bodyExtent.y : 0.0f) +
                                     actionGap + actionHeight;
            const float height = ImMax(iconRadius * 2.0f, textHeight) + padY * 2.0f;

            const int placementIndex = PlacementIndex(resolved.placement);
            const float stackOffset = placementIndex >= 0 ? stackOffsets[placementIndex] : 0.0f;
            const ImVec2 minimum = Place(resolved, *viewport, ImVec2(width, height), stackOffset);
            const ImVec2 maximum = minimum + ImVec2(width, height);
            const bool hovered = ImGui::IsMouseHoveringRect(minimum, maximum);

            if (state.alive && !(hovered && resolved.pauseOnHover))
                state.age += dt;
            if (resolved.duration > 0.0f && state.age >= resolved.duration)
                state.alive = false;

            ElementOptions element;
            element.role = ElementRole::Notification;
            if (state.options.effects)
                element.effects = &*state.options.effects;
            if (state.options.motion)
                element.motion = &*state.options.motion;
            ElementScope scope = BeginElement(draw,
                                              ImHashData(&state.handle.value,
                                                         sizeof(state.handle.value)),
                                              state.alive, element);
            if (!scope)
            {
                finished.push_back(state.handle);
                continue;
            }

            const float rounding = island ? height * 0.5f :
                (compact ? theme.RoundingControl * 0.7f : theme.RoundingControl) * scale;
            const ImU32 accent = TypeColor(resolved.type, theme);
            ImU32 fill = WithAlphaMul(theme.Surface1, 0.98f);
            if (glass)
                fill = WithAlphaMul(theme.GlassTint, 0.46f);
            else if (compact)
                fill = WithAlphaMul(theme.Surface2, 0.98f);
            else if (island)
                fill = IM_COL32(12, 12, 15, 246);
            draw->AddRectFilled(minimum, maximum, fill, rounding);
            if (island || glass)
                Sheen(draw, minimum, maximum, rounding, island ? 0.5f : theme.Sheen);
            if (!island)
            {
                draw->AddRect(minimum, maximum, WithAlphaMul(theme.Border, 0.95f),
                              rounding, 0, theme.BorderThickness * scale);
                if (banner)
                    draw->AddRectFilled(minimum,
                        ImVec2(maximum.x, minimum.y + 3.0f * scale), accent,
                        rounding, ImDrawFlags_RoundCornersTop);
                else if (!compact)
                    draw->AddRectFilled(minimum,
                        ImVec2(minimum.x + 4.0f * scale, maximum.y), accent,
                        rounding, ImDrawFlags_RoundCornersLeft);
            }

            const float contentStart = minimum.x + padX +
                (island || compact ? 0.0f : 4.0f * scale);
            const ImVec2 iconCenter(contentStart + iconRadius,
                                    minimum.y + (height - textHeight) * 0.5f + titleExtent.y * 0.5f);
            draw->AddCircleFilled(iconCenter, iconRadius, accent);
            const char* glyph = TypeGlyph(resolved.type);
            const ImVec2 glyphSize = CalcTextScaled(glyph, theme.FontSizeSmall, titleFont);
            AddTextScaled(draw, iconCenter - glyphSize * 0.5f,
                          island ? IM_COL32(12, 12, 15, 255) : IM_COL32(20, 22, 26, 255),
                          glyph, theme.FontSizeSmall, titleFont);

            const float textX = contentStart + iconRadius * 2.0f + iconGap;
            float textY = minimum.y + (height - textHeight) * 0.5f;
            AddWrapped(draw, titleFont, titleSize, ImVec2(textX, textY),
                       island ? IM_COL32(245, 245, 248, 255) : theme.Text,
                       state.title, textWidth);
            textY += titleExtent.y;
            if (hasBody)
            {
                textY += lineGap;
                AddWrapped(draw, bodyFont, bodySize, ImVec2(textX, textY),
                           island ? IM_COL32(182, 184, 192, 255) : theme.TextMuted,
                           state.body, textWidth);
                textY += bodyExtent.y;
            }

            bool actionClicked = false;
            if (hasAction)
            {
                textY += actionGap;
                const ImVec2 actionMin(textX, textY);
                const ImVec2 actionMax(textX + naturalAction.x, textY + actionHeight);
                const bool actionHovered = ImGui::IsMouseHoveringRect(actionMin, actionMax);
                AddWrapped(draw, titleFont, bodySize, actionMin,
                           actionHovered ? theme.AccentHover : accent,
                           state.actionLabel, textWidth);
                actionClicked = actionHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                if (actionClicked)
                {
                    pendingActions.push_back({state.handle, state.options.action->callback,
                                              state.options.action->userData});
                    if (state.options.action->dismiss)
                        state.alive = false;
                }
            }

            if (!actionClicked && hovered && resolved.dismissOnClick &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                state.alive = false;

            if (resolved.showProgress)
            {
                float progress = state.options.progress.value_or(
                    resolved.duration > 0.0f
                        ? ImClamp(1.0f - state.age / resolved.duration, 0.0f, 1.0f)
                        : 1.0f);
                progress = ImClamp(progress, 0.0f, 1.0f);
                const float inset = island ? rounding * 0.65f : rounding;
                const float barY = maximum.y - 2.5f * scale;
                draw->AddLine(ImVec2(minimum.x + inset, barY),
                              ImVec2(minimum.x + inset + (width - inset * 2.0f) * progress, barY),
                              WithAlphaMul(accent, 0.9f), 2.5f * scale);
            }

            if (placementIndex >= 0)
                stackOffsets[placementIndex] += height + resolved.spacing;
        }

        for (NotificationHandle handle : finished)
            for (auto iterator = context->notifications.begin();
                 iterator != context->notifications.end(); ++iterator)
                if (iterator->handle == handle)
                {
                    context->notifications.erase(iterator);
                    break;
                }

        for (const PendingAction& action : pendingActions)
            if (action.callback)
                action.callback(action.handle, action.userData);
    }
}
