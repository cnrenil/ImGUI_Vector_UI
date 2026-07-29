/// ImGuiVectorUI — vector icons drawn with ImDrawList primitives.
/// No raster textures: every icon is strokes/arcs, so it tints with the theme,
/// animates, and stays crisp at any scale (and needs no DX12 descriptor plumbing).
#pragma once

#include "imgui.h"

namespace IVUI
{
    enum class IconType
    {
        Crosshair, ///<  Aimbot
        Eye,       ///<  ESP
        Bolt,      ///<  Weapon
        User,      ///<  Player
        Star,      ///<  Visual
        Gear,      ///<  Config
        Sliders,
        Check,
    };

    /// `size` is the icon box diameter; positions scale from it about `center`.
#ifdef IVUI_INTERNAL_API
    void IconCheck(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconChevron(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness, int dir); ///<  0=down 1=up 2=left 3=right
    void IconCrosshair(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconEye(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconBolt(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconUser(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconStar(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconGear(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);
    void IconSliders(ImDrawList* draw, ImVec2 center, float size, ImU32 col, float thickness);

    void Icon(ImDrawList* draw, IconType type, ImVec2 center, float size, ImU32 col, float thickness);
#endif
}
