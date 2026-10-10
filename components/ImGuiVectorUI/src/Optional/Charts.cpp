#include "ImGuiVectorUI/Optional/Charts.h"

#include "ImGuiVectorUI/Core/Context.h"

#include "imgui_internal.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace IVUI
{
    namespace
    {
        ImVec4 Color(ImU32 value)
        {
            return ImGui::ColorConvertU32ToFloat4(value);
        }

        ImU32 Alpha(ImU32 value, float alpha)
        {
            const int valueAlpha = static_cast<int>(
                ((value >> IM_COL32_A_SHIFT) & 0xffu) * ImClamp(alpha, 0.0f, 1.0f));
            return (value & ~IM_COL32_A_MASK) |
                   (static_cast<ImU32>(valueAlpha) << IM_COL32_A_SHIFT);
        }

        ImU32 SeriesColor(const Theme& theme, std::size_t index)
        {
            const ImU32 palette[] = {
                theme.Accent,
                theme.AccentHover,
                theme.AccentActive,
                theme.Knob,
                theme.TrackOn,
                theme.TextMuted};
            return palette[index % (sizeof(palette) / sizeof(palette[0]))];
        }

        struct PlotStyleScope
        {
            int colorCount = 0;
            int styleCount = 0;

            explicit PlotStyleScope(const Theme& theme)
            {
                ImPlot::PushStyleColor(ImPlotCol_FrameBg, theme.Surface1);
                ImPlot::PushStyleColor(ImPlotCol_PlotBg, theme.Surface0);
                ImPlot::PushStyleColor(ImPlotCol_PlotBorder, theme.Border);
                ImPlot::PushStyleColor(ImPlotCol_LegendBg, theme.Surface1);
                ImPlot::PushStyleColor(ImPlotCol_LegendBorder, theme.Border);
                ImPlot::PushStyleColor(ImPlotCol_LegendText, theme.Text);
                ImPlot::PushStyleColor(ImPlotCol_TitleText, theme.Text);
                ImPlot::PushStyleColor(ImPlotCol_InlayText, theme.Text);
                ImPlot::PushStyleColor(ImPlotCol_AxisText, theme.TextMuted);
                ImPlot::PushStyleColor(ImPlotCol_AxisGrid, theme.BorderMuted);
                ImPlot::PushStyleColor(ImPlotCol_AxisTick, theme.BorderMuted);
                ImPlot::PushStyleColor(ImPlotCol_AxisBg, Alpha(theme.Surface1, 0.28f));
                ImPlot::PushStyleColor(ImPlotCol_AxisBgHovered, theme.HoverTint);
                ImPlot::PushStyleColor(ImPlotCol_AxisBgActive, theme.AccentMuted);
                ImPlot::PushStyleColor(ImPlotCol_Selection, theme.AccentMuted);
                ImPlot::PushStyleColor(ImPlotCol_Crosshairs, theme.Accent);
                colorCount = 16;

                ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize,
                                     (std::max)(theme.BorderThickness * theme.Scale, 0.0f));
                ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding,
                                     ImVec2(theme.ControlPadding.x * theme.Scale,
                                            theme.ControlPadding.y * theme.Scale));
                styleCount = 2;
            }

            ~PlotStyleScope()
            {
                ImPlot::PopStyleVar(styleCount);
                ImPlot::PopStyleColor(colorCount);
            }
        };

        struct PlotContextScope
        {
            ImPlotContext* previous = nullptr;

            explicit PlotContextScope(ImPlotContext* context)
                : previous(ImPlot::GetCurrentContext())
            {
                ImPlot::SetCurrentContext(context);
            }

            ~PlotContextScope()
            {
                ImPlot::SetCurrentContext(previous);
            }
        };

        ImPlotContext* CreateOwnedContext(void*& previous)
        {
            previous = ImPlot::GetCurrentContext();
            return ImPlot::CreateContext();
        }

        struct PreparedSeries
        {
            const ChartSeriesView* view = nullptr;
            std::size_t sourceIndex = 0;
            std::size_t first = 0;
            std::size_t count = 0;
        };

        std::vector<PreparedSeries> PrepareSeries(std::span<const ChartSeriesView> series,
                                                   const ChartStreamOptions* stream)
        {
            std::vector<PreparedSeries> prepared;
            prepared.reserve(series.size());
            const std::size_t maxInt = static_cast<std::size_t>(
                (std::numeric_limits<int>::max)());
            for (std::size_t index = 0; index < series.size(); ++index)
            {
                const ChartSeriesView& view = series[index];
                if (!view.label || !view.values || view.count == 0)
                    continue;

                std::size_t first = 0;
                if (stream && stream->maxPoints != 0 && view.count > stream->maxPoints)
                    first = view.count - stream->maxPoints;
                const std::size_t count =
                    (std::min)(view.count - first, maxInt);
                if (count != 0)
                    prepared.push_back({&view, index, first, count});
            }
            return prepared;
        }

        double XValue(const PreparedSeries& series, std::size_t index)
        {
            const std::size_t sourceIndex = series.first + index;
            if (series.view->xValues)
                return series.view->xValues[sourceIndex];
            return series.view->xStart +
                   series.view->xStep * static_cast<double>(sourceIndex);
        }

        bool FindXRange(const std::vector<PreparedSeries>& series,
                       double& minimum, double& maximum)
        {
            bool found = false;
            for (const PreparedSeries& prepared : series)
                for (std::size_t index = 0; index < prepared.count; ++index)
                {
                    const double x = XValue(prepared, index);
                    if (!std::isfinite(x))
                        continue;
                    if (!found)
                    {
                        minimum = maximum = x;
                        found = true;
                    }
                    else
                    {
                        minimum = (std::min)(minimum, x);
                        maximum = (std::max)(maximum, x);
                    }
                }
            if (found && minimum == maximum)
            {
                minimum -= 0.5;
                maximum += 0.5;
            }
            return found;
        }

        ImPlotSpec MakeSpec(const Theme& theme, std::size_t seriesIndex,
                            const ChartOptions& options, ChartType type)
        {
            const ImU32 color = SeriesColor(theme, seriesIndex);
            ImPlotSpec spec;
            spec.LineColor = Color(color);
            spec.LineWeight = (std::max)(options.lineWeight, 0.0f);
            spec.FillColor = Color(color);
            spec.FillAlpha = ImClamp(options.areaAlpha, 0.0f, 1.0f);
            spec.MarkerLineColor = Color(color);
            spec.MarkerFillColor = Color(color);
            spec.MarkerSize = (std::max)(options.markerSize, 0.0f);
            if (type == ChartType::Scatter)
                spec.Marker = ImPlotMarker_Circle;
            return spec;
        }

        void RenderSeries(ChartType type, const PreparedSeries& prepared,
                          const Theme& theme, const ChartOptions& options)
        {
            const ChartSeriesView& view = *prepared.view;
            const double* values = view.values + prepared.first;
            const int count = static_cast<int>(prepared.count);
            ImPlotSpec spec = MakeSpec(theme, prepared.sourceIndex, options, type);

            if (type == ChartType::Bar)
            {
                std::vector<double> x(prepared.count);
                for (std::size_t index = 0; index < prepared.count; ++index)
                    x[index] = XValue(prepared, index) + options.barShift;
                ImPlot::PlotBars(view.label, x.data(), values, count,
                                 (std::max)(options.barWidth, 0.001), spec);
            }
            else if (type == ChartType::Area)
            {
                if (view.xValues)
                    ImPlot::PlotShaded(view.label, view.xValues + prepared.first, values,
                                       count, options.areaBaseline, spec);
                else
                    ImPlot::PlotShaded(view.label, values, count, options.areaBaseline,
                                       view.xStep,
                                       view.xStart + view.xStep * prepared.first, spec);
            }
            else if (type == ChartType::Scatter)
            {
                if (view.xValues)
                    ImPlot::PlotScatter(view.label, view.xValues + prepared.first, values,
                                        count, spec);
                else
                    ImPlot::PlotScatter(view.label, values, count, view.xStep,
                                        view.xStart + view.xStep * prepared.first, spec);
            }
            else if (view.xValues)
            {
                ImPlot::PlotLine(view.label, view.xValues + prepared.first, values,
                                 count, spec);
            }
            else
            {
                ImPlot::PlotLine(view.label, values, count, view.xStep,
                                 view.xStart + view.xStep * prepared.first, spec);
            }
        }

        ChartHoverInfo FindHover(const std::vector<PreparedSeries>& series,
                                 float hoverRadius)
        {
            ChartHoverInfo result;
            result.hovered = ImPlot::IsPlotHovered();
            if (!result.hovered)
                return result;

            const ImVec2 mouse = ImGui::GetMousePos();
            float bestDistance = (std::max)(hoverRadius, 0.0f);
            bestDistance *= bestDistance;
            for (const PreparedSeries& prepared : series)
                for (std::size_t index = 0; index < prepared.count; ++index)
                {
                    const double x = XValue(prepared, index);
                    const double y = prepared.view->values[prepared.first + index];
                    if (!std::isfinite(x) || !std::isfinite(y))
                        continue;
                    const ImVec2 pixel = ImPlot::PlotToPixels(x, y);
                    const float dx = pixel.x - mouse.x;
                    const float dy = pixel.y - mouse.y;
                    const float distance = dx * dx + dy * dy;
                    if (distance > bestDistance)
                        continue;

                    bestDistance = distance;
                    result.hasPoint = true;
                    result.seriesIndex = prepared.sourceIndex;
                    result.pointIndex = prepared.first + index;
                    result.seriesLabel = prepared.view->label;
                    result.x = x;
                    result.y = y;
                    result.pixel = pixel;
                }
            return result;
        }

        void DrawTooltip(const Theme& theme, const ChartHoverInfo& hover)
        {
            if (!hover.hasPoint)
                return;
            ImGui::PushStyleColor(ImGuiCol_PopupBg, theme.Surface1);
            ImGui::PushStyleColor(ImGuiCol_Border, theme.Border);
            ImGui::PushStyleColor(ImGuiCol_Text, theme.Text);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                theme.RoundingControl * theme.Scale);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                 ImVec2(theme.ControlPadding.x * theme.Scale,
                                        theme.ControlPadding.y * theme.Scale));
            if (ImGui::BeginTooltip())
            {
                if (hover.seriesLabel)
                    ImGui::TextUnformatted(hover.seriesLabel);
                ImGui::Text("x: %.6g", hover.x);
                ImGui::Text("y: %.6g", hover.y);
                ImGui::EndTooltip();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        void DrawHoverMarker(const Theme& theme, const ChartHoverInfo& hover,
                             std::size_t seriesIndex)
        {
            if (!hover.hasPoint)
                return;
            ImPlot::PushPlotClipRect();
            ImDrawList* drawList = ImPlot::GetPlotDrawList();
            const ImU32 color = SeriesColor(theme, seriesIndex);
            drawList->AddCircleFilled(hover.pixel, (std::max)(3.0f, theme.Scale * 3.0f), color);
            drawList->AddCircle(hover.pixel, (std::max)(5.0f, theme.Scale * 5.0f),
                                theme.Text, 16, (std::max)(theme.Scale, 1.0f));
            ImPlot::PopPlotClipRect();
        }
    }

    struct ChartsContext::State
    {
        struct PlotState
        {
            bool fitted = false;
            bool fitRequested = false;
        };

        ImGuiContext* imguiContext = nullptr;
        std::unordered_map<ImGuiID, PlotState> plots;

        void Prepare()
        {
            ImGuiContext* current = ImGui::GetCurrentContext();
            if (current != imguiContext)
            {
                plots.clear();
                imguiContext = current;
            }
        }
    };

    ChartsContext::ChartsContext()
        : context_(CreateOwnedContext(previousContext_)),
          state_(std::make_unique<State>())
    {
        ImPlot::SetCurrentContext(static_cast<ImPlotContext*>(context_));
    }

    ChartsContext::~ChartsContext()
    {
        ImPlotContext* context = static_cast<ImPlotContext*>(context_);
        if (context)
        {
            ImPlotContext* current = ImPlot::GetCurrentContext();
            ImPlot::DestroyContext(context);
            if (current == context)
                ImPlot::SetCurrentContext(static_cast<ImPlotContext*>(previousContext_));
            context_ = nullptr;
        }
    }

    std::unique_ptr<ChartsContext> ChartsContext::Create()
    {
        return std::unique_ptr<ChartsContext>(new ChartsContext());
    }

    bool ChartsContext::Plot(const char* id, ChartType type,
                             std::span<const ChartSeriesView> series,
                             const ChartOptions& options, const Theme* theme)
    {
        return Render(type, id, series, nullptr, options, theme);
    }

    bool ChartsContext::Line(const char* id, std::span<const ChartSeriesView> series,
                             const ChartOptions& options, const Theme* theme)
    {
        return Plot(id, ChartType::Line, series, options, theme);
    }

    bool ChartsContext::Bar(const char* id, std::span<const ChartSeriesView> series,
                            const ChartOptions& options, const Theme* theme)
    {
        return Plot(id, ChartType::Bar, series, options, theme);
    }

    bool ChartsContext::Area(const char* id, std::span<const ChartSeriesView> series,
                             const ChartOptions& options, const Theme* theme)
    {
        return Plot(id, ChartType::Area, series, options, theme);
    }

    bool ChartsContext::Scatter(const char* id, std::span<const ChartSeriesView> series,
                                const ChartOptions& options, const Theme* theme)
    {
        return Plot(id, ChartType::Scatter, series, options, theme);
    }

    bool ChartsContext::Stream(const char* id, std::span<const ChartSeriesView> series,
                               const ChartOptions& options, const Theme* theme)
    {
        return Stream(id, series, ChartStreamOptions{}, options, theme);
    }

    bool ChartsContext::Stream(const char* id, std::span<const ChartSeriesView> series,
                               const ChartStreamOptions& stream,
                               const ChartOptions& options, const Theme* theme)
    {
        return Render(ChartType::Line, id, series, &stream, options, theme);
    }

    bool ChartsContext::RealTime(const char* id, std::span<const ChartSeriesView> series,
                                 const ChartStreamOptions& stream,
                                 const ChartOptions& options, const Theme* theme)
    {
        return Stream(id, series, stream, options, theme);
    }

    bool ChartsContext::Fit(const char* id)
    {
        if (!context_ || !state_ || !id || !ImGui::GetCurrentContext())
            return false;
        state_->Prepare();
        state_->plots[ImGui::GetID(id)].fitRequested = true;
        return true;
    }

    bool ChartsContext::Render(ChartType type, const char* id,
                               std::span<const ChartSeriesView> series,
                               const ChartStreamOptions* stream,
                               const ChartOptions& options, const Theme* theme)
    {
        if (!context_ || !state_ || !id || !ImGui::GetCurrentContext())
            return false;

        state_->Prepare();
        PlotContextScope plotContext(static_cast<ImPlotContext*>(context_));
        const ImGuiID chartId = ImGui::GetID(id);
        State::PlotState& plotState = state_->plots[chartId];
        const Theme* activeTheme = theme;
        if (!activeTheme)
        {
            Context* context = GetContext();
            if (!context || context->themeStack.empty())
                return false;
            activeTheme = &context->themeStack.back();
        }
        const Theme& chartTheme = *activeTheme;
        const std::vector<PreparedSeries> prepared = PrepareSeries(series, stream);

        if (options.feedback)
            *options.feedback = {};

        const bool fit = (options.fitAxes && !plotState.fitted) ||
                         plotState.fitRequested;
        if (fit)
            ImPlot::SetNextAxesToFit();
        if (stream && stream->maxPoints != 0 && stream->followLatest)
        {
            double xMinimum = 0.0;
            double xMaximum = 0.0;
            if (FindXRange(prepared, xMinimum, xMaximum))
                ImPlot::SetNextAxisLimits(ImAxis_X1, xMinimum, xMaximum,
                                          ImPlotCond_Always);
        }

        ElementOptions elementOptions;
        elementOptions.role = ElementRole::Chart;
        ElementVisualStyle explicitVisual;
        if (options.element)
            elementOptions = *options.element;
        else if (theme)
        {
            explicitVisual = theme->ResolveElement(ElementRole::Chart);
            elementOptions.effects = &explicitVisual.effects;
            elementOptions.motion = &explicitVisual.motion;
        }

        ItemMotionStyle noEntryMotion;
        if (!options.animateEntry)
        {
            noEntryMotion = ItemMotionStyle::Fade(0.0f);
            noEntryMotion.animateInitial = false;
            elementOptions.motion = &noEntryMotion;
        }

        ElementScope element = BeginElement(ImGui::GetWindowDrawList(), chartId, true,
                                            elementOptions);
        if (options.feedback)
            options.feedback->entryProgress = element.Progress();
        if (!element)
            return false;

        ImPlotFlags flags = ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText;
        if (!options.showLegend)
            flags |= ImPlotFlags_NoLegend;
        if (!options.enableZoomPan)
            flags |= ImPlotFlags_NoInputs;

        PlotStyleScope style(chartTheme);
        if (!ImPlot::BeginPlot(id, options.size, flags))
            return false;

        const ImPlotAxisFlags axisFlags = options.showGrid
            ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoGridLines;
        ImPlot::SetupAxes(options.xLabel, options.yLabel, axisFlags, axisFlags);
        if (options.showLegend)
            ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_None);
        ImPlot::SetupFinish();

        for (const PreparedSeries& preparedSeries : prepared)
            RenderSeries(type, preparedSeries, chartTheme, options);

        ChartHoverInfo hover = FindHover(prepared, options.hoverRadius * chartTheme.Scale);
        if (options.showHoverMarker && hover.hasPoint)
            DrawHoverMarker(chartTheme, hover, hover.seriesIndex);
        if (options.showTooltip && hover.hasPoint)
            DrawTooltip(chartTheme, hover);

        ImPlot::EndPlot();
        if (options.feedback)
            options.feedback->hover = hover;

        if (fit && !prepared.empty())
        {
            plotState.fitted = true;
            plotState.fitRequested = false;
        }
        return true;
    }
}
