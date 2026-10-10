/** @file Charts.h @brief ImPlot-backed chart module. */
#pragma once

#include "imgui.h"
#include "ImGuiVectorUI/Core/Element.h"
#include "ImGuiVectorUI/Theme/Theme.h"

#include <cstddef>
#include <memory>
#include <span>

namespace IVUI
{
    enum class ChartType
    {
        Line,
        Bar,
        Area,
        Scatter
    };

    struct ChartSeriesView
    {
        const char* label = nullptr;
        const double* values = nullptr;
        std::size_t count = 0;
        double xStart = 0.0;
        double xStep = 1.0;
        const double* xValues = nullptr; ///< Optional explicit x coordinates.
    };

    /** Non-owning IVUI data view used by chart and stream APIs. */
    using ChartDataView = ChartSeriesView;
    using ChartStreamView = ChartSeriesView;

    struct ChartHoverInfo
    {
        static constexpr std::size_t InvalidIndex = static_cast<std::size_t>(-1);

        bool hovered = false;
        bool hasPoint = false;
        std::size_t seriesIndex = InvalidIndex;
        std::size_t pointIndex = InvalidIndex;
        const char* seriesLabel = nullptr;
        double x = 0.0;
        double y = 0.0;
        ImVec2 pixel{};
    };

    struct ChartFeedback
    {
        float entryProgress = 1.0f;
        ChartHoverInfo hover{};
    };

    using ChartHoverState = ChartHoverInfo;

    struct ChartOptions
    {
        ImVec2 size{0.0f, 220.0f};
        bool showLegend = true;
        bool fitAxes = true; ///< Fit on first data-bearing draw; use Fit() to refit.
        bool showGrid = true;
        const ElementOptions* element = nullptr;
        bool enableZoomPan = true;
        bool showTooltip = true;
        bool showHoverMarker = true;
        bool animateEntry = true;
        float lineWeight = 2.0f;
        float markerSize = 4.0f;
        float areaAlpha = 0.24f;
        float hoverRadius = 14.0f;
        double barWidth = 0.67;
        double barShift = 0.0;
        double areaBaseline = 0.0;
        const char* xLabel = nullptr;
        const char* yLabel = nullptr;
        ChartFeedback* feedback = nullptr;
    };

    struct ChartStreamOptions
    {
        /** Zero renders all samples; otherwise the newest maxPoints are shown. */
        std::size_t maxPoints = 0;
        /** Keep the x-axis at the newest samples while the stream advances. */
        bool followLatest = true;
    };

    /** Owns one ImPlot context and keeps ImPlot out of the portable IVUI API. */
    class ChartsContext final
    {
    public:
        static std::unique_ptr<ChartsContext> Create();
        ~ChartsContext();

        ChartsContext(ChartsContext&&) = delete;
        ChartsContext& operator=(ChartsContext&&) = delete;
        ChartsContext(const ChartsContext&) = delete;
        ChartsContext& operator=(const ChartsContext&) = delete;

        bool Plot(const char* id, ChartType type, std::span<const ChartSeriesView> series,
                  const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool Line(const char* id, std::span<const ChartSeriesView> series,
                  const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool Bar(const char* id, std::span<const ChartSeriesView> series,
                 const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool Area(const char* id, std::span<const ChartSeriesView> series,
                  const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool Scatter(const char* id, std::span<const ChartSeriesView> series,
                     const ChartOptions& options = {}, const Theme* theme = nullptr);

        bool Stream(const char* id, std::span<const ChartSeriesView> series,
                    const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool Stream(const char* id, std::span<const ChartSeriesView> series,
                    const ChartStreamOptions& stream,
                    const ChartOptions& options = {}, const Theme* theme = nullptr);
        bool RealTime(const char* id, std::span<const ChartSeriesView> series,
                      const ChartStreamOptions& stream,
                      const ChartOptions& options = {}, const Theme* theme = nullptr);

        /** Requests a fit on the next Plot/Line/Bar/Area/Scatter/Stream call. */
        bool Fit(const char* id);

    private:
        struct State;
        ChartsContext();
        bool Render(ChartType type, const char* id,
                    std::span<const ChartSeriesView> series,
                    const ChartStreamOptions* stream,
                    const ChartOptions& options, const Theme* theme);
        void* context_ = nullptr;
        void* previousContext_ = nullptr;
        std::unique_ptr<State> state_;
    };
}
