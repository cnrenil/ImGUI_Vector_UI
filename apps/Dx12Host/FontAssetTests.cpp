#include "imgui.h"

#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc != 2)
        return EXIT_FAILURE;

    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImFont* base = io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    ImFont* merged = io.Fonts->AddFontFromFileTTF(
        argv[1], 18.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
    const bool built = merged != nullptr && io.Fonts->Build();
    const bool hasChinese = built && base->IsGlyphInFont(0x4E2D);

    ImGui::DestroyContext(context);
    return hasChinese ? EXIT_SUCCESS : EXIT_FAILURE;
}
