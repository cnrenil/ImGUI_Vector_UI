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
    static_assert(std::is_trivially_copyable_v<IVUI::ItemEffectRequest>);
    static_assert(IVUI::HasEffectCapabilities(
        IVUI::AllEffectCapabilities,
        IVUI::RequiredEffectCapabilities(IVUI::EffectId::Glass)));
    static_assert(IVUI::HasEffectCapabilities(
        IVUI::AllEffectCapabilities,
        IVUI::EffectCapabilities::ItemSourceReplacement));
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
    glass.shapePower = 4.6f;
    glass.lensStrength = 2.3f;
    glass.lensBase = 5.2f;
    glass.lensFalloff = 6.9f;
    glass.lensBias = 0.7f;
    glass.lensPower = 1.2f;
    const IVUI::EffectRequest request = IVUI::EffectRequest::Glass(glass);
    const IVUI::EffectConstants constants =
        IVUI::MakeGlassEffectConstants(request.glass, {800, 600}, 3.0f);

    Check(request.id == IVUI::EffectId::Glass);
    Check(constants.values[0] == 12.0f);
    Check(constants.values[3] == 180.0f);
    Check(constants.values[6] == 1.0f);
    Check(constants.values[17] == 4.6f);
    Check(constants.values[18] == 2.3f);
    Check(constants.values[19] == 5.2f);
    Check(constants.values[20] == 6.9f);
    Check(constants.values[21] == 0.7f);
    Check(constants.values[22] == 1.2f);
    Check(constants.values[28] == 1.0f / 800.0f);
    Check(constants.values[30] == 3.0f);

    IVUI::ItemEffectRequest item{};
    Check(item.Add(IVUI::ItemEffectLayer::Shadow(
        {0.0f, 0.0f, 0.0f, 0.8f}, {2.0f, 4.0f}, 6.0f, 0.5f)));
    Check(item.Add(IVUI::ItemEffectLayer::Glow(color, 8.0f, 0.7f)));
    Check(item.Add(IVUI::ItemEffectLayer::Blur(3.0f)));
    Check(item.layerCount == 3);
    Check(IVUI::HasItemEffectFlags(item.layers[2].flags,
                                   IVUI::ItemEffectFlags::ReplaceSource));
    return EXIT_SUCCESS;
}
