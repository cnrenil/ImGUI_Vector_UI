#include "ImGuiVectorUI/Optional/Charts.h"

#include "ImGuiVectorUI/Core/Context.h"

#include "imgui.h"

#include <cstdio>
#include <cstdlib>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "Chart check failed: %s\n", message);
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    ImGuiContext* imgui = ImGui::CreateContext();
    Check(imgui != nullptr, "ImGui context");
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800.0f, 4000.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* fontPixels = nullptr;
    int fontWidth = 0;
    int fontHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);

    IVUI::Context* ivui = IVUI::CreateContext();
    Check(ivui != nullptr, "IVUI context");
    ivui->frameActive = true;

    auto charts = IVUI::ChartsContext::Create();
    Check(charts != nullptr, "charts context");

    const double values[] = {1.0, 2.0, 1.5, 3.0, 2.5};
    const double explicitX[] = {10.0, 11.0, 12.0, 13.0, 14.0};
    const IVUI::ChartSeriesView series[] = {
        {"Primary", values, 5, 0.0, 1.0},
        {"Explicit X", values, 5, 0.0, 1.0, explicitX}};

    ImGui::NewFrame();
    IVUI::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(600.0f, 1800.0f), ImGuiCond_Always);
    ImGui::Begin("Charts", nullptr, ImGuiWindowFlags_NoSavedSettings);

    IVUI::ChartFeedback feedback;
    IVUI::ChartOptions options;
    options.showTooltip = false;
    options.showHoverMarker = false;
    options.feedback = &feedback;
    Check(charts->Line("line", series, options), "line API");
    Check(feedback.entryProgress >= 0.0f && feedback.entryProgress <= 1.0f,
          "entry progress");
    Check(charts->Bar("bar", series, options), "bar API");
    Check(charts->Area("area", series, options), "area API");
    Check(charts->Scatter("scatter", series, options), "scatter API");
    Check(charts->Plot("generic", IVUI::ChartType::Line, series, options),
          "generic API");

    IVUI::ChartStreamOptions stream;
    stream.maxPoints = 3;
    stream.followLatest = true;
    Check(charts->Fit("stream"), "fit request");
    Check(charts->Stream("stream", series, stream, options), "stream API");
    Check(charts->RealTime("realtime", series, stream, options), "realtime API");

    ImGui::End();
    ImGui::Render();
    ivui->frameActive = false;
    charts.reset();
    IVUI::DestroyContext(ivui);
    ImGui::DestroyContext(imgui);
    return EXIT_SUCCESS;
}
