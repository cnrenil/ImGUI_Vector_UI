#include "ImGuiVectorUI/Drawing/Icons.h"

#include "imgui_internal.h" // IM_PI

#include <math.h>

namespace IVUI
{
    static inline ImVec2 P(ImVec2 c, float s, float x, float y)
    {
        return ImVec2(c.x + x * s, c.y + y * s);
    }

    void IconCheck(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        ImVec2 pts[3] = { P(c, s, -0.34f, 0.02f), P(c, s, -0.10f, 0.26f), P(c, s, 0.36f, -0.28f) };
        draw->AddPolyline(pts, 3, col, 0, th);
    }

    void IconChevron(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th, int dir)
    {
        ImVec2 a, b, d;
        switch (dir)
        {
        case 1: a = P(c, s, -0.4f, 0.15f); b = P(c, s, 0.0f, -0.2f); d = P(c, s, 0.4f, 0.15f); break;  // up
        case 2: a = P(c, s, 0.15f, -0.4f); b = P(c, s, -0.2f, 0.0f); d = P(c, s, 0.15f, 0.4f); break;  // left
        case 3: a = P(c, s, -0.15f, -0.4f); b = P(c, s, 0.2f, 0.0f); d = P(c, s, -0.15f, 0.4f); break; // right
        default: a = P(c, s, -0.4f, -0.15f); b = P(c, s, 0.0f, 0.2f); d = P(c, s, 0.4f, -0.15f); break; // down
        }
        ImVec2 pts[3] = { a, b, d };
        draw->AddPolyline(pts, 3, col, 0, th);
    }

    void IconCrosshair(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        const float r = 0.42f * s;
        draw->AddCircle(c, r, col, 0, th);
        const float t0 = r * 0.55f, t1 = r * 1.2f;
        draw->AddLine(ImVec2(c.x, c.y - t0), ImVec2(c.x, c.y - t1), col, th);
        draw->AddLine(ImVec2(c.x, c.y + t0), ImVec2(c.x, c.y + t1), col, th);
        draw->AddLine(ImVec2(c.x - t0, c.y), ImVec2(c.x - t1, c.y), col, th);
        draw->AddLine(ImVec2(c.x + t0, c.y), ImVec2(c.x + t1, c.y), col, th);
        draw->AddCircleFilled(c, th * 0.9f, col);
    }

    void IconEye(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        ImVec2 l = P(c, s, -0.45f, 0.0f);
        ImVec2 r = P(c, s, 0.45f, 0.0f);
        draw->AddBezierQuadratic(l, P(c, s, 0.0f, -0.62f), r, col, th);
        draw->AddBezierQuadratic(l, P(c, s, 0.0f, 0.62f), r, col, th);
        draw->AddCircleFilled(c, 0.15f * s, col);
    }

    void IconBolt(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        ImVec2 pts[6] = {
            P(c, s, 0.10f, -0.5f), P(c, s, -0.26f, 0.06f), P(c, s, 0.0f, 0.06f),
            P(c, s, -0.1f, 0.5f),  P(c, s, 0.28f, -0.08f), P(c, s, 0.02f, -0.08f)
        };
        draw->AddPolyline(pts, 6, col, ImDrawFlags_Closed, th);
    }

    void IconUser(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        draw->AddCircle(P(c, s, 0.0f, -0.2f), 0.18f * s, col, 0, th);
        draw->PathClear();
        draw->PathArcTo(P(c, s, 0.0f, 0.55f), 0.34f * s, IM_PI, IM_PI * 2.0f, 24);
        draw->PathStroke(col, 0, th);
    }

    void IconStar(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        ImVec2 pts[10];
        const float outer = 0.48f * s, inner = 0.2f * s;
        for (int i = 0; i < 10; ++i)
        {
            const float rad = (i & 1) ? inner : outer;
            const float ang = -IM_PI * 0.5f + (float)i * (IM_PI / 5.0f);
            pts[i] = ImVec2(c.x + cosf(ang) * rad, c.y + sinf(ang) * rad);
        }
        draw->AddPolyline(pts, 10, col, ImDrawFlags_Closed, th);
    }

    void IconGear(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        const float r = 0.34f * s;
        draw->AddCircle(c, r, col, 20, th);
        draw->AddCircle(c, 0.15f * s, col, 12, th);
        for (int i = 0; i < 8; ++i)
        {
            const float ang = (float)i * (IM_PI / 4.0f);
            const float cx = cosf(ang), cy = sinf(ang);
            draw->AddLine(ImVec2(c.x + cx * r, c.y + cy * r),
                          ImVec2(c.x + cx * r * 1.32f, c.y + cy * r * 1.32f), col, th);
        }
    }

    void IconSliders(ImDrawList* draw, ImVec2 c, float s, ImU32 col, float th)
    {
        const float x0 = -0.42f * s, x1 = 0.42f * s;
        const float ys[3] = { -0.26f * s, 0.0f, 0.26f * s };
        const float knobX[3] = { 0.18f * s, -0.12f * s, 0.30f * s };
        for (int i = 0; i < 3; ++i)
        {
            draw->AddLine(ImVec2(c.x + x0, c.y + ys[i]), ImVec2(c.x + x1, c.y + ys[i]), col, th);
            draw->AddCircleFilled(ImVec2(c.x + knobX[i], c.y + ys[i]), 0.09f * s, col);
        }
    }

    void Icon(ImDrawList* draw, IconType type, ImVec2 c, float s, ImU32 col, float th)
    {
        switch (type)
        {
        case IconType::Crosshair: IconCrosshair(draw, c, s, col, th); break;
        case IconType::Eye:       IconEye(draw, c, s, col, th); break;
        case IconType::Bolt:      IconBolt(draw, c, s, col, th); break;
        case IconType::User:      IconUser(draw, c, s, col, th); break;
        case IconType::Star:      IconStar(draw, c, s, col, th); break;
        case IconType::Gear:      IconGear(draw, c, s, col, th); break;
        case IconType::Sliders:   IconSliders(draw, c, s, col, th); break;
        case IconType::Check:     IconCheck(draw, c, s, col, th); break;
        }
    }
}
