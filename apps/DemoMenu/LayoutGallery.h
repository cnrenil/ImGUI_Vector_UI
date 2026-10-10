#pragma once

#include "ImGuiVectorUI/UI.h"

namespace Demo
{
    class LayoutGallery
    {
    public:
        void Render(IVUI::UI& ui);
    private:
        int mode_ = 0;
        bool open_[4] = {true, true, true, true};
        bool enabled_ = true;
        float intensity_ = 0.65f;
        int quality_ = 1;
        char name_[128] = "Workspace";
        ImVec4 color_{0.32f, 0.81f, 0.63f, 1.0f};
    };
}
