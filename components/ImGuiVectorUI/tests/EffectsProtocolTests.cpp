#include "ImGuiVectorUI/Drawing/Effects.h"

#include <cstdlib>
#include <type_traits>

namespace
{
    void Check(bool condition)
    {
        if (!condition)
            std::exit(EXIT_FAILURE);
    }
}

int main()
{
    static_assert(std::is_trivially_copyable_v<IVUI::EffectRequest>);
    static_assert(IVUI::HasEffectCapabilities(
        IVUI::AllEffectCapabilities,
        IVUI::RequiredEffectCapabilities(IVUI::EffectId::Glass)));
    static_assert(!IVUI::HasEffectCapabilities(
        IVUI::EffectCapabilities::GlassMaterial,
        IVUI::RequiredEffectCapabilities(IVUI::EffectId::Glass)));

    constexpr IVUI::EffectColor color = IVUI::EffectColorFromRgba8(0xff804020u);
    static_assert(color.r == 1.0f);
    static_assert(color.a > 0.12f && color.a < 0.13f);

    IVUI::GlassEffectParams glass{};
    glass.bounds = {{12.0f, 24.0f}, {240.0f, 180.0f}};
    glass.blurSigma = 7.5f;
    glass.tint = color;
    const IVUI::EffectRequest request = IVUI::EffectRequest::Glass(glass);
    const IVUI::EffectConstants constants =
        IVUI::MakeGlassEffectConstants(request.glass, {800, 600}, 3.0f);

    Check(request.id == IVUI::EffectId::Glass);
    Check(constants.values[0] == 12.0f);
    Check(constants.values[3] == 180.0f);
    Check(constants.values[6] == 1.0f);
    Check(constants.values[28] == 1.0f / 800.0f);
    Check(constants.values[30] == 3.0f);
    return EXIT_SUCCESS;
}
