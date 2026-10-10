#pragma once

namespace IVUI { class UI; }

namespace Demo
{
    /** Sets the directory containing runtime demo assets, including the sample model. */
    void SetAssetDirectory(const char* directory);
    void RenderMenu(IVUI::UI& ui, bool* open);
}
