// ImGuiVectorUI — animated sliders (ported from MYGui::AnimatedSlider ×3).
#pragma once

#include <initializer_list>

namespace IVUI
{
    // Continuous float slider with an eased fill + knob.
    bool SliderFloat(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f");

    // Stepped slider (segment dots). Writes/reads the integer *v in [0, segments-1].
    int SliderSegments(const char* label, int* v, int segments, const char* fmt = "%d");

    // Discrete labeled slider with spring snap-to-nearest. *v indexes opts.
    int SliderLabeled(const char* label, int* v, std::initializer_list<const char*> opts, const char* fmt = "%s");
}
