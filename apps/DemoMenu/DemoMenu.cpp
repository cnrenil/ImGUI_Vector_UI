/** @file DemoMenu.cpp @brief Backend-neutral interactive ImGuiVectorUI demo menu. */
#include "DemoMenu.h"
#include "LayoutGallery.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiVectorUI/ImGuiVectorUI.h"
#include "ImGuiVectorUI/Core/DragDrop.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Drawing/Tooltip.h"
#include "ImGuiVectorUI/Optional/Charts.h"
#include "ImGuiVectorUI/Optional/ModelViewer.h"

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace Demo
{
    static IVUI::UI* g_ui = nullptr;
    static int   g_tab = 6;
    static LayoutGallery g_layoutGallery;
    static int g_density = 0;
    static bool g_reducedMotion = false;
    static float g_motionSpeed = 1.0f;
    static float g_menuVis = 0.0f;

    static bool  g_aimEnabled = true;
    static float g_fov = 75.0f, g_smooth = 8.0f;
    static int   g_aimMode = 1, g_aimBone = 0;
    static bool  g_aimVisCheck = true;

    static bool   g_espBox = true, g_espName = true, g_espDist = false, g_espHealth = true;
    static float  g_espDistance = 300.0f;
    static ImVec4 g_espColor = ImVec4(1.0f, 0.30f, 0.30f, 1.0f);

    static int   g_weaponMode = 0, g_fireRate = 2;
    static bool  g_noRecoil = true;

    static bool  g_godmode = false, g_infAmmo = true, g_speedhack = false;
    static float g_speedMult = 1.5f;

    static int   g_quality = 1;
    static bool  g_nightMode = false, g_statusHudEnabled = false;
    static float g_bloom = 1.0f;
    static float g_bloomThreshold = 0.08f;
    static float g_bloomSaturation = 0.04f;
    static float g_bloomSigma = 3.0f;
    static float g_backdropBlurSigma = 16.0f;

    static float g_scrimAlpha = 0.32f;

    static int    g_menuKey = ImGuiKey_Insert;
    static int    g_style = 5;
    static float  g_scale = 1.0f, g_roundMul = 1.0f;
    static float  g_glowMul = 1.0f, g_translucencyMul = 1.0f; // free-adjust multipliers
    static bool   g_motionPreview = true;
    static bool   g_effectMotionOpen = true, g_effectItemOpen = true;
    static bool   g_effectTextOpen = true, g_effectNotificationOpen = true;
    static bool   g_effectComponentsOpen = true;
    static bool   g_effectChartsOpen = true, g_effectModelViewerOpen = true;
    static std::string g_assetDirectory;
    static std::unique_ptr<IVUI::ChartsContext> g_charts;
    static const double g_chartPrimary[] = { 0.18, 0.26, 0.31, 0.48, 0.42, 0.66, 0.71, 0.84, 0.78, 0.96 };
    static const double g_chartSecondary[] = { 0.64, 0.58, 0.69, 0.55, 0.73, 0.67, 0.82, 0.76, 0.91, 0.86 };
    static double g_chartStream[32]{};
    static IVUI::GltfScene g_previewScene;
    static IVUI::ModelViewerState g_modelViewerState;
    static bool g_previewSceneReady = false;
    static bool g_previewSceneAttempted = false;
    static std::string g_previewSceneError;
    static ImVec4 g_accent = ImVec4(1.0f, 180.0f / 255.0f, 169.0f / 255.0f, 1.0f);
    static ImVec4 g_glassTint = ImVec4(0.11f, 0.12f, 0.16f, 1.0f);
    static float  g_glassTintStrength = 0.16f, g_glassOpacity = 0.82f;
    static float  g_glassRefraction = 1.25f, g_glassEdgeWidth = 2.0f;
    static float  g_glassEdgeHighlight = 0.22f, g_glassThickness = 0.35f;
    static float  g_glassShadow = 0.50f;
    static float  g_glassShapePower = 4.0f, g_glassLensStrength = 0.30f, g_glassLensBase = 5.2f;
    static float  g_glassLensFalloff = 6.9f, g_glassLensBias = 0.7f, g_glassLensPower = 1.0f, g_glassNoise = 0.0f;
    static bool   g_cfgKeybinds = true, g_streamproof = false;
    static IVUI::Keybind g_kbAim{ ImGuiKey_C }, g_kbEsp{ ImGuiKey_E }, g_kbPanic{ ImGuiKey_Delete };

    static void ApplyNotificationAction(IVUI::NotificationHandle, void*)
    {
        g_weaponMode = (g_weaponMode + 1) % 3;
    }

    void SetAssetDirectory(const char* directory)
    {
        g_assetDirectory = directory ? directory : "";
        g_previewScene = {};
        g_previewSceneReady = false;
        g_previewSceneAttempted = false;
        g_previewSceneError.clear();
    }

    static bool EnsurePreviewScene()
    {
        if (g_previewSceneReady)
            return true;
        if (g_previewSceneAttempted)
            return false;

        g_previewSceneAttempted = true;
        std::string path = g_assetDirectory;
        if (!path.empty() && path.back() != '\\' && path.back() != '/')
            path.push_back('\\');
        path += "CesiumMilkTruck.glb";
        if (!IVUI::LoadGltf(path.c_str(), g_previewScene, &g_previewSceneError))
        {
            if (g_previewSceneError.empty())
                g_previewSceneError = "Unable to load the sample model.";
            return false;
        }
        g_previewSceneReady = true;
        return true;
    }

    struct Feature { std::string id, category, group, name; std::function<void(const char*)> draw; };
    struct Category { std::string id, name; IVUI::IconType icon; };

    static std::vector<Category> g_categories;
    static std::vector<Feature>  g_features;

    static Feature* FindFeature(const std::string& id)
    {
        for (auto& f : g_features)
            if (f.id == id)
                return &f;
        return nullptr;
    }

    static void DrawEntityTable()
    {
        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;
        struct Ent { const char* name; float dist; int hp; };
        static Ent ents[] = {
            { "Bandit", 42.5f, 80 }, { "Skag", 12.0f, 100 }, { "Psycho", 88.3f, 45 },
            { "Loader", 150.0f, 100 }, { "Badass", 30.0f, 100 }, { "Varkid", 64.0f, 60 } };
        const int N = (int)(sizeof(ents) / sizeof(ents[0]));
        if (g_ui->layouts().BeginTable("##ents", 3, ImVec2(0.0f, 140.0f * S)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Dist");
            ImGui::TableSetupColumn("HP");
            ImGui::TableHeadersRow();
            if (ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs())
                if (ss->SpecsDirty && ss->SpecsCount > 0)
                {
                    const ImGuiTableColumnSortSpecs& s = ss->Specs[0];
                    std::sort(ents, ents + N, [&](const Ent& a, const Ent& b) {
                        int c = 0;
                        if (s.ColumnIndex == 0) c = strcmp(a.name, b.name);
                        else if (s.ColumnIndex == 1) c = (a.dist < b.dist) ? -1 : (a.dist > b.dist) ? 1 : 0;
                        else c = a.hp - b.hp;
                        return s.SortDirection == ImGuiSortDirection_Ascending ? c < 0 : c > 0;
                    });
                    ss->SpecsDirty = false;
                }
            for (int i = 0; i < N; ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(ents[i].name);
                ImGui::TableNextColumn(); ImGui::Text("%.1f m", ents[i].dist);
                ImGui::TableNextColumn(); ImGui::Text("%d", ents[i].hp);
            }
            g_ui->layouts().EndTable();
        }
    }

    static void RegisterFeatures()
    {
        if (!g_features.empty())
            return;
        g_categories = {
            { "aimbot", "Aimbot", IVUI::IconType::Crosshair },
            { "esp",    "ESP",    IVUI::IconType::Eye },
            { "weapon", "Weapon", IVUI::IconType::Bolt },
            { "player", "Player", IVUI::IconType::User },
            { "visual", "Visual", IVUI::IconType::Star },
        };
        auto add = [&](const char* cat, const char* grp, const char* id, const char* name, std::function<void(const char*)> d) {
            g_features.push_back({ std::string(cat) + "." + id, cat, grp, name, std::move(d) });
        };

        add("aimbot", "", "enabled", "Enabled", [](const char* l) { g_ui->widgets().Toggle(l, &g_aimEnabled); });
        add("aimbot", "", "fov", "FOV", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_fov, 1.0f, 180.0f, "%.0f"); });
        add("aimbot", "", "smoothing", "Smoothing", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_smooth, 1.0f, 20.0f, "%.1f"); });
        add("aimbot", "", "mode", "Mode", [](const char* l) { g_ui->widgets().SliderLabeled(l, &g_aimMode, { "Off", "Legit", "Rage" }); });
        add("aimbot", "Advanced", "vischeck", "Visibility Check", [](const char* l) { g_ui->widgets().Toggle(l, &g_aimVisCheck); });
        add("aimbot", "Advanced", "bone", "Target Bone", [](const char* l) { g_ui->widgets().Combo(l, &g_aimBone, { "Head", "Neck", "Chest", "Pelvis" }); });

        add("esp", "", "boxes", "Boxes", [](const char* l) {
            g_ui->widgets().ComboMulti(l, { { "2D Box", &g_espBox }, { "Name", &g_espName }, { "Distance", &g_espDist }, { "Health Bar", &g_espHealth } });
        });
        add("esp", "", "distance", "Max Distance", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_espDistance, 10.0f, 1000.0f, "%.0f m"); });
        add("esp", "", "color", "Box Color", [](const char* l) { g_ui->widgets().ColorPicker(l, &g_espColor); });
        add("esp", "", "entities", "Entity List", [](const char*) { DrawEntityTable(); });

        add("weapon", "", "mode", "Fire Mode", [](const char* l) { g_ui->widgets().Combo(l, &g_weaponMode, { "Legit", "Rage", "Hybrid" }); });
        add("weapon", "", "firerate", "Fire Rate", [](const char* l) { g_ui->widgets().SliderSegments(l, &g_fireRate, 5, "%d"); });
        add("weapon", "", "norecoil", "No Recoil", [](const char* l) { g_ui->widgets().Toggle(l, &g_noRecoil); });
        add("weapon", "", "apply", "Apply Loadout", [](const char* l) { if (g_ui->widgets().Button(l, {.prominent = true})) g_weaponMode = (g_weaponMode + 1) % 3; });

        add("player", "", "god", "God Mode", [](const char* l) { g_ui->widgets().Toggle(l, &g_godmode); });
        add("player", "", "ammo", "Infinite Ammo", [](const char* l) { g_ui->widgets().Toggle(l, &g_infAmmo); });
        add("player", "", "speed", "Speed Hack", [](const char* l) { g_ui->widgets().Toggle(l, &g_speedhack); });
        add("player", "", "speedmult", "Speed Multiplier", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_speedMult, 1.0f, 5.0f, "%.2fx"); });

        add("visual", "", "quality", "Quality", [](const char* l) { g_ui->widgets().SliderLabeled(l, &g_quality, { "Low", "Medium", "High", "Ultra" }); });
        add("visual", "", "bloom", "Bloom Strength", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_bloom, 0.0f, 2.0f, "%.2f"); });
        add("visual", "Bloom", "bloom_threshold", "Bloom Threshold", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_bloomThreshold, 0.0f, 1.0f, "%.2f"); });
        add("visual", "Bloom", "bloom_saturation", "Bloom Saturation", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_bloomSaturation, 0.0f, 0.5f, "%.2f"); });
        add("visual", "Bloom", "bloom_sigma", "Bloom Sigma", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_bloomSigma, 0.5f, 3.0f, "%.2f"); });
        add("visual", "", "night", "Night Mode", [](const char* l) { g_ui->widgets().Toggle(l, &g_nightMode); });
        add("visual", "", "feature_hud", "Feature HUD", [](const char* l) { g_ui->widgets().Toggle(l, &g_statusHudEnabled); });

        add("visual", "Glass Effect", "glass_scrim", "Backdrop Dim", [](const char* l) { g_ui->widgets().SliderFloat(l, &g_scrimAlpha, 0.0f, 0.85f, "%.2f"); });

    }

    struct PanelItem { std::string featureId; std::string customName; }; // customName overrides the label
    struct UserPanel { std::string id, name; ImVec2 pos; bool open = true; std::vector<PanelItem> items; };
    static std::vector<UserPanel> g_panels;
    static std::string g_editTarget;    // panel id currently in edit mode ("" = none)
    static std::string g_pendingDelete;
    static char g_renameBuf[64];        // panel title rename buffer
    static char g_featRenameBuf[64];    // panel-item rename buffer
    static std::string g_featRenameKey; // "panelId|featureId" of the item being renamed
    static bool g_featRenameFocus = false;
    static int g_panelCounter = 0;
    static bool g_seeded = false;

    static bool PanelHasFeature(const UserPanel& p, const std::string& fid)
    {
        for (auto& it : p.items) if (it.featureId == fid) return true;
        return false;
    }
    static void InsertFeatureToPanel(const std::string& panelId, const std::string& fid, int index)
    {
        for (auto& p : g_panels)
            if (p.id == panelId)
            {
                if (PanelHasFeature(p, fid)) return; // dedup
                if (index < 0) index = 0;
                if (index > (int)p.items.size()) index = (int)p.items.size();
                p.items.insert(p.items.begin() + index, PanelItem{ fid, "" });
                return;
            }
    }
    static void AddFeatureToPanel(const std::string& panelId, const std::string& fid)
    {
        for (auto& p : g_panels)
            if (p.id == panelId) { InsertFeatureToPanel(panelId, fid, (int)p.items.size()); return; }
    }
    static void AddCategoryToPanel(const std::string& panelId, const std::string& cat)
    {
        for (auto& f : g_features)
            if (f.category == cat)
                AddFeatureToPanel(panelId, f.id);
    }
    static void CreatePanel(const ImGuiViewport* vp, float S)
    {
        UserPanel p;
        p.id = "p" + std::to_string(g_panelCounter);
        p.name = "Panel " + std::to_string(g_panelCounter + 1);
        const int k = g_panelCounter % 6;
        p.pos = ImVec2(vp->Pos.x + (60.0f + k * 28.0f) * S, vp->Pos.y + (90.0f + k * 28.0f) * S);
        g_panelCounter++;
        g_panels.push_back(std::move(p));
    }

    static void DrawGrip(ImDrawList* dl, ImVec2 c, ImU32 col, float S)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float y = c.y + (i - 1) * 4.0f * S;
            dl->AddCircleFilled(ImVec2(c.x - 2.0f * S, y), 1.3f * S, col);
            dl->AddCircleFilled(ImVec2(c.x + 2.0f * S, y), 1.3f * S, col);
        }
    }
    static void DrawCloseIcon(ImDrawList* dl, ImVec2 c, ImU32 col, float S)
    {
        const float g = 4.5f * S, t = ImMax(1.6f * S, 1.0f);
        dl->AddLine(ImVec2(c.x - g, c.y - g), ImVec2(c.x + g, c.y + g), col, t);
        dl->AddLine(ImVec2(c.x - g, c.y + g), ImVec2(c.x + g, c.y - g), col, t);
    }

    static void Heading(const char* text)
    {
        const IVUI::Theme& th = g_ui->theme().Current();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c = ImGui::GetCursorScreenPos();
        IVUI::AddTextScaled(dl, c, th.Accent, text, th.FontSizeTitle, IVUI::GetTitleFont());
        const ImVec2 s = IVUI::CalcTextScaled(text, th.FontSizeTitle, IVUI::GetTitleFont());
        ImGui::Dummy(ImVec2(s.x, s.y + 8.0f * th.Scale));
    }

    static void ApplyTheme()
    {
        IVUI::Theme base = g_style < 5 ? IVUI::GetStyle((IVUI::Style)g_style)
            : IVUI::MakeTheme(g_style == 6 ? IVUI::ColorScheme::Light : IVUI::ColorScheme::Dark,
                             g_density ? IVUI::Density::Compact : IVUI::Density::Comfortable);
        g_ui->motion().Configure({g_motionSpeed, g_reducedMotion});
        static int prevStyle = -1;
        if (g_style != prevStyle)
        {
            g_accent = ImGui::ColorConvertU32ToFloat4(base.Accent);
            g_glassTint = ImGui::ColorConvertU32ToFloat4(base.GlassTint);
            g_glassTintStrength = base.GlassTintStrength;
            g_glassOpacity = base.GlassOpacity;
            g_glassRefraction = base.GlassRefraction;
            g_glassEdgeWidth = base.GlassEdgeWidth;
            g_glassEdgeHighlight = base.GlassEdgeHighlight;
            g_glassThickness = base.GlassThickness;
            g_glassShadow = base.PanelShadow;
            g_glassShapePower = base.GlassShapePower;
            g_glassLensStrength = base.GlassLensStrength;
            g_glassLensBase = base.GlassLensBase;
            g_glassLensFalloff = base.GlassLensFalloff;
            g_glassLensBias = base.GlassLensBias;
            g_glassLensPower = base.GlassLensPower;
            g_glassNoise = base.GlassNoise;
            g_bloomThreshold = base.BloomThreshold;
            g_bloomSaturation = base.BloomSaturation;
            g_bloomSigma = base.BloomSigma;
            g_backdropBlurSigma = base.BackdropBlurSigma;
            prevStyle = g_style;
        }
        IVUI::SetAccent(base, ImGui::ColorConvertFloat4ToU32(g_accent));
        base.Scale = g_scale * g_ui->theme().ContentScale();
        base.RoundingPanel *= g_roundMul;
        base.RoundingControl *= g_roundMul;
        base.RoundingPill *= g_roundMul;
        base.WidgetGlow *= g_glowMul;
        base.PanelGlow *= g_glowMul * g_bloom;
        base.BloomThreshold = g_bloomThreshold;
        base.BloomSaturation = g_bloomSaturation;
        base.BloomSigma = g_bloomSigma;
        base.BackdropBlurSigma = g_backdropBlurSigma;
        base.SurfaceAlpha = ImClamp(base.SurfaceAlpha * g_translucencyMul, 0.0f, 1.0f);
        base.PanelBodyAlpha = ImClamp(base.PanelBodyAlpha * g_translucencyMul, 0.0f, 1.0f);
        base.GlassTint = ImGui::ColorConvertFloat4ToU32(g_glassTint);
        base.GlassTintStrength = ImClamp(g_glassTintStrength, 0.0f, 1.0f);
        base.GlassOpacity = ImClamp(g_glassOpacity * g_translucencyMul, 0.0f, 1.0f);
        base.GlassRefraction = ImClamp(g_glassRefraction, 0.0f, 8.0f);
        base.GlassEdgeWidth = ImClamp(g_glassEdgeWidth, 0.5f, 8.0f);
        base.GlassEdgeHighlight = ImClamp(g_glassEdgeHighlight, 0.0f, 1.0f);
        base.GlassThickness = ImClamp(g_glassThickness, 0.0f, 1.0f);
        base.PanelShadow = ImClamp(g_glassShadow, 0.0f, 1.0f);
        base.GlassShapePower = ImClamp(g_glassShapePower, 2.0f, 8.0f);
        base.GlassLensStrength = ImClamp(g_glassLensStrength, 0.0f, 4.0f);
        base.GlassLensBase = ImClamp(g_glassLensBase, 1.01f, 8.0f);
        base.GlassLensFalloff = ImClamp(g_glassLensFalloff, 0.1f, 12.0f);
        base.GlassLensBias = ImClamp(g_glassLensBias, 0.0f, 4.0f);
        base.GlassLensPower = ImClamp(g_glassLensPower, 0.1f, 4.0f);
        base.GlassNoise = ImClamp(g_glassNoise, 0.0f, 3.0f);
        g_ui->theme().SetTarget(base);
    }

    static void MainFeatureRow(const Feature& f)
    {
        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;

        if (g_editTarget.empty())
        {
            ImGui::PushID(f.id.c_str());
            f.draw(f.name.c_str());
            ImGui::PopID();
            return;
        }

        const float gutterW = 20.0f * S;
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float availW = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::PushID(f.id.c_str());
        ImGui::Indent(gutterW);
        f.draw(f.name.c_str());
        ImGui::Unindent(gutterW);
        float rowH = ImGui::GetCursorScreenPos().y - cursor.y - ImGui::GetStyle().ItemSpacing.y;
        if (rowH < th.ControlHeight * S * 0.5f) rowH = th.ControlHeight * S * 0.5f;

        const bool rowHov = ImGui::IsMouseHoveringRect(cursor, ImVec2(cursor.x + availW, cursor.y + rowH));
        dl->AddRectFilled(cursor, ImVec2(cursor.x + availW, cursor.y + rowH),
                          IVUI::WithAlphaMul(th.Accent, rowHov ? 0.20f : 0.08f), th.RoundingControl * S);

        ImGui::SetCursorScreenPos(cursor);
        ImGui::InvisibleButton("grip", ImVec2(gutterW, rowH));
        const bool gripHov = ImGui::IsItemHovered();
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("IVUI_FEAT", f.id.c_str(), f.id.size() + 1);
            IVUI::DrawDragGhost(f.name.c_str());
            ImGui::EndDragDropSource();
        }
        DrawGrip(dl, ImVec2(cursor.x + gutterW * 0.5f, cursor.y + rowH * 0.5f), gripHov ? th.Accent : th.TextMuted, S);
        { char tip[96]; snprintf(tip, sizeof(tip), "%s · 拖到面板", f.name.c_str()); IVUI::Tooltip(tip); }

        ImGui::SetCursorScreenPos(ImVec2(cursor.x + gutterW, cursor.y));
        const bool clicked = ImGui::InvisibleButton("addhit", ImVec2(availW - gutterW, rowH));
        { char tip[96]; snprintf(tip, sizeof(tip), "点击加入 %s", f.name.c_str()); IVUI::Tooltip(tip); }
        if (clicked) AddFeatureToPanel(g_editTarget, f.id);
        ImGui::PopID();
    }

    /** @brief Draws one feature row inside an editable panel.
     *  @return True when the caller should remove the feature.
     */
    static bool PanelFeatureRow(const std::string& panelId, PanelItem& item, int index, bool editMode)
    {
        const Feature* f = FindFeature(item.featureId);
        if (!f) return false;
        const std::string displayName = item.customName.empty() ? f->name : item.customName;
        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;
        ImGui::PushID(item.featureId.c_str());

        if (!editMode)
        {
            f->draw(displayName.c_str());
            ImGui::PopID();
            return false;
        }

        const float leftG = 20.0f * S, rightG = 22.0f * S;
        const float childW = ImMax(ImGui::GetContentRegionAvail().x - leftG - rightG, 40.0f * S);
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float availW = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const std::string renameKey = panelId + "|" + item.featureId;
        const bool renaming = (g_featRenameKey == renameKey);
        float rowH;

        if (renaming)
        {
            ImGui::SetCursorScreenPos(ImVec2(cursor.x + leftG, cursor.y));
            ImGui::SetNextItemWidth(childW);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, th.Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text, th.Text);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, th.RoundingControl * S);
            if (g_featRenameFocus) { ImGui::SetKeyboardFocusHere(); g_featRenameFocus = false; }
            const bool enter = ImGui::InputText("##fr", g_featRenameBuf, sizeof(g_featRenameBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool committed = enter || ImGui::IsItemDeactivatedAfterEdit();
            const bool closed = committed || ImGui::IsItemDeactivated();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            if (committed)
            {
                std::string nn = g_featRenameBuf;
                const size_t a = nn.find_first_not_of(" \t");
                const size_t b = nn.find_last_not_of(" \t");
                nn = (a == std::string::npos) ? std::string() : nn.substr(a, b - a + 1);
                item.customName = (nn == f->name) ? std::string() : nn; // same as default -> clear
            }
            if (closed) g_featRenameKey.clear();
            rowH = ImGui::GetFrameHeight();
            ImGui::SetCursorScreenPos(cursor);
            ImGui::Dummy(ImVec2(availW, rowH));
        }
        else
        {
            ImGui::Indent(leftG);
            // NoInputs transfers edit-mode input to the parent-level overlay.
            ImGui::BeginChild("fw", ImVec2(childW, 0.0f), ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            f->draw(displayName.c_str());
            ImGui::EndChild();
            ImGui::Unindent(leftG);
            rowH = ImGui::GetCursorScreenPos().y - cursor.y - ImGui::GetStyle().ItemSpacing.y;
            if (rowH < th.ControlHeight * S * 0.5f) rowH = th.ControlHeight * S * 0.5f;

            ImGui::SetCursorScreenPos(ImVec2(cursor.x + leftG, cursor.y));
            ImGui::InvisibleButton("block", ImVec2(childW, rowH));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                g_featRenameKey = renameKey;
                g_featRenameFocus = true;
                std::snprintf(g_featRenameBuf, sizeof(g_featRenameBuf), "%s", displayName.c_str());
            }
            IVUI::Tooltip("双击重命名");
        }

        bool remove = false;
        ImGui::SetCursorScreenPos(cursor);
        if (ImGui::InvisibleButton("rm", ImVec2(leftG, rowH))) remove = true;
        DrawCloseIcon(dl, ImVec2(cursor.x + leftG * 0.5f, cursor.y + rowH * 0.5f), ImGui::IsItemHovered() ? th.Accent : th.TextMuted, S);
        IVUI::Tooltip("从面板移除");

        const ImVec2 rpos(cursor.x + availW - rightG, cursor.y);
        ImGui::SetCursorScreenPos(rpos);
        ImGui::InvisibleButton("reorder", ImVec2(rightG, rowH));
        DrawGrip(dl, ImVec2(rpos.x + rightG * 0.5f, cursor.y + rowH * 0.5f), ImGui::IsItemHovered() ? th.Accent : th.TextMuted, S);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("IVUI_REORDER", &index, sizeof(int));
            IVUI::DrawDragGhost(displayName.c_str());
            ImGui::EndDragDropSource();
        }
        IVUI::Tooltip("拖动调整顺序");

        ImGui::PopID();
        return remove;
    }

    static bool g_grpOpen[8] = { true, true, true, true, true, true, true, true };

    static void RenderCategoryContent(const Category& cat)
    {
        Heading(cat.name.c_str());
        if (!g_editTarget.empty())
        {
            if (g_ui->widgets().Button("+ Add whole category"))
                AddCategoryToPanel(g_editTarget, cat.id);
        }
        for (auto& f : g_features)
            if (f.category == cat.id && f.group.empty())
                MainFeatureRow(f);
        std::vector<std::string> groups;
        for (auto& f : g_features)
            if (f.category == cat.id && !f.group.empty() &&
                std::find(groups.begin(), groups.end(), f.group) == groups.end())
                groups.push_back(f.group);
        int gi = 0;
        for (const auto& grp : groups)
        {
            const int slot = gi++ % 8;
            g_ui->layouts().Section(grp.c_str(), &g_grpOpen[slot], [&] {
                for (auto& f : g_features)
                    if (f.category == cat.id && f.group == grp)
                        MainFeatureRow(f);
            });
        }
    }

    static void RenderEffectsPreviewTab()
    {
        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;
        Heading("Effects Preview");

        g_ui->layouts().Section("Motion", &g_effectMotionOpen, [&] {
            g_ui->widgets().Toggle("Preview motion", &g_motionPreview);
            auto motion = g_ui->motion().BeginItem(
                "effects_motion_preview", g_motionPreview,
                th.ResolveElement(IVUI::ElementRole::Default).motion);
            if (motion)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, th.Accent);
                ImGui::TextUnformatted("Fade + slide item motion");
                ImGui::PopStyleColor();
            }
        });

        g_ui->layouts().Section("Item Effects", &g_effectItemOpen, [&] {
            IVUI::ItemEffectRequest customEffects{};
            customEffects.Add(IVUI::ItemEffectLayer::Shadow(
                {0.0f, 0.0f, 0.0f, 0.75f}, {0.0f, 5.0f}, 7.0f, 0.45f));
            const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(th.Accent);
            customEffects.Add(IVUI::ItemEffectLayer::Glow(
                {accent.x, accent.y, accent.z, accent.w}, 8.0f, 0.55f));

            IVUI::ButtonOptions customButton{};
            customButton.element.effects = &customEffects;
            const IVUI::ItemMotionStyle buttonMotion =
                th.ResolveElement(IVUI::ElementRole::Button).motion;
            customButton.element.motion = &buttonMotion;
            g_ui->widgets().Button("Shadow + Glow", customButton);
            ImGui::SameLine();
            g_ui->widgets().Button("Theme default", {});

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 size(220.0f * S, 42.0f * S);
            ImGui::Dummy(size);
            IVUI::ElementOptions elementOptions;
            elementOptions.role = IVUI::ElementRole::Panel;
            elementOptions.effects = &customEffects;
            auto element = g_ui->elements().Begin(
                ImGui::GetWindowDrawList(), ImGui::GetID("effects_custom_geometry"), true,
                elementOptions);
            if (element)
            {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, cursor + size, th.Surface2, th.RoundingControl * S);
                draw->AddRect(cursor, cursor + size, th.Accent,
                              th.RoundingControl * S, 0, th.BorderThickness * S);
                IVUI::AddTextScaled(draw, cursor + ImVec2(14.0f * S, 10.0f * S),
                                    th.Text, "Arbitrary element geometry", th.FontSizeSmall);
            }
        });

        g_ui->layouts().Section("Components: defaults and element overrides",
                                &g_effectComponentsOpen, [&] {
            static bool overrideElement = true;
            g_ui->widgets().Toggle("Element override", &overrideElement);

            IVUI::ItemEffectRequest pickerEffects{};
            const ImVec4 pickerAccent = ImGui::ColorConvertU32ToFloat4(th.Accent);
            pickerEffects.Add(IVUI::ItemEffectLayer::Glow(
                {pickerAccent.x, pickerAccent.y, pickerAccent.z, pickerAccent.w},
                10.0f, 0.65f));
            IVUI::ElementOptions pickerElement;
            pickerElement.role = IVUI::ElementRole::ColorPicker;
            pickerElement.effects = &pickerEffects;
            const IVUI::ElementVisualStyle pickerVisual =
                th.ResolveElement(IVUI::ElementRole::ColorPicker);
            pickerElement.motion = &pickerVisual.motion;

            ImVec4 defaultColor = g_accent;
            ImVec4 overrideColor = g_glassTint;
            g_ui->widgets().ColorPicker("Theme default color", &defaultColor);
            IVUI::ColorPickerOptions overrideOptions;
            overrideOptions.element = overrideElement ? &pickerElement : nullptr;
            g_ui->widgets().ColorPicker("Element override color", &overrideColor,
                                        overrideOptions);

            ImGui::SeparatorText("Default controls");
            g_ui->widgets().Button("Default button");
            ImGui::SameLine();
            g_ui->widgets().Toggle("Default toggle", &g_motionPreview);
            ImGui::SameLine();
            int galleryMode = 0;
            g_ui->widgets().Combo("Default combo", &galleryMode, {"Default", "Active", "Muted"});

            ImGui::SeparatorText("Overridden geometry");
            const ImVec2 galleryCursor = ImGui::GetCursorScreenPos();
            const ImVec2 gallerySize(260.0f * S, 42.0f * S);
            ImGui::Dummy(gallerySize);
            auto galleryElement = g_ui->elements().Begin(
                ImGui::GetWindowDrawList(), ImGui::GetID("component_gallery_override"), true,
                pickerElement);
            if (galleryElement)
            {
                ImDrawList* galleryDraw = ImGui::GetWindowDrawList();
                galleryDraw->AddRectFilled(galleryCursor, galleryCursor + gallerySize,
                                           th.Surface2, th.RoundingControl * S);
                galleryDraw->AddText(galleryCursor + ImVec2(14.0f * S, 12.0f * S),
                                     th.Text, "Element-level effect override");
            }
        });

        g_ui->layouts().Section("Animated Text", &g_effectTextOpen, [&] {
            IVUI::AnimatedTextOptions rainbowWave;
            rainbowWave.animation = IVUI::TextAnimation::Rainbow | IVUI::TextAnimation::Wave;
            rainbowWave.speed = 0.35f;
            rainbowWave.waveAmplitude = 3.5f;
            g_ui->widgets().AnimatedText("Rainbow + wave text##rainbow_wave", rainbowWave);

            IVUI::AnimatedTextOptions gradientPulse;
            gradientPulse.animation = IVUI::TextAnimation::GradientFlow | IVUI::TextAnimation::ColorPulse;
            gradientPulse.speed = 0.6f;
            g_ui->widgets().AnimatedText("Theme gradient pulse##gradient_pulse", gradientPulse);

            IVUI::AnimatedTextOptions typewriter;
            typewriter.animation = IVUI::TextAnimation::Typewriter;
            typewriter.typewriterDuration = 2.2f;
            g_ui->widgets().AnimatedText("Typewriter animation##typewriter", typewriter);

            IVUI::AnimatedTextOptions flickerFade;
            flickerFade.animation = IVUI::TextAnimation::Flicker | IVUI::TextAnimation::FadeIn;
            flickerFade.fadeDuration = 1.0f;
            flickerFade.fadeStagger = 0.45f;
            g_ui->widgets().AnimatedText("Flicker + fade-in##flicker_fade", flickerFade);
        });

        g_ui->layouts().Section("Notifications", &g_effectNotificationOpen, [&] {
            if (g_ui->widgets().Button("Info"))
                g_ui->widgets().Notify("提示 Info", "这是一条信息通知");
            if (g_ui->widgets().Button("Dynamic Island", {.prominent = true}))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Success;
                options.style = IVUI::NotificationStyle::DynamicIsland;
                options.placement = IVUI::NotificationPlacement::TopCenter;
                g_ui->widgets().Notify("成功 Success", "灵动岛是通知的逐实例样式参数", options);
            }
            if (g_ui->widgets().Button("Top Left Warning"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Warning;
                options.placement = IVUI::NotificationPlacement::TopLeft;
                g_ui->widgets().Notify("警告 Warning", "此通知独立覆盖主题位置", options);
            }
            if (g_ui->widgets().Button("Glass"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Info;
                options.style = IVUI::NotificationStyle::Glass;
                g_ui->widgets().Notify("Glass notification", "Translucent themed surface", options);
            }
            if (g_ui->widgets().Button("Compact"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Success;
                options.style = IVUI::NotificationStyle::Compact;
                g_ui->widgets().Notify("Compact", "Small notification layout", options);
            }
            if (g_ui->widgets().Button("Banner"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Warning;
                options.style = IVUI::NotificationStyle::Banner;
                options.placement = IVUI::NotificationPlacement::TopCenter;
                g_ui->widgets().Notify("Banner notification", "Full-width placement with a top accent", options);
            }
            if (g_ui->widgets().Button("Error"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Error;
                g_ui->widgets().Notify("错误 Error", "发生了一个错误", options);
            }
            if (g_ui->widgets().Button("Action + Progress"))
            {
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Info;
                options.placement = IVUI::NotificationPlacement::BottomCenter;
                options.progress = 0.68f;
                options.duration = 0.0f;
                options.action = IVUI::NotificationAction{"Apply", ApplyNotificationAction, nullptr, true};
                g_ui->widgets().Notify("Loadout ready", "A persistent notification with an action", options);
            }
        });

        g_ui->layouts().Section("Charts", &g_effectChartsOpen, [&] {
            if (!g_charts)
                g_charts = IVUI::ChartsContext::Create();
            const IVUI::ChartSeriesView series[] = {
                {"Primary", g_chartPrimary, IM_ARRAYSIZE(g_chartPrimary), 0.0, 1.0},
                {"Secondary", g_chartSecondary, IM_ARRAYSIZE(g_chartSecondary), 0.0, 1.0}};
            IVUI::ChartOptions options;
            options.size = ImVec2(0.0f, 170.0f * S);
            options.showTooltip = true;
            g_charts->Line("phase4_chart_line", series, options, &th);
            g_charts->Bar("phase4_chart_bar", series, options, &th);
            g_charts->Area("phase4_chart_area", series, options, &th);
            g_charts->Scatter("phase4_chart_scatter", series, options, &th);

            const float time = static_cast<float>(ImGui::GetTime());
            for (int index = 0; index < IM_ARRAYSIZE(g_chartStream); ++index)
                g_chartStream[index] = 0.5 + 0.4 * std::sin(time * 2.0 + index * 0.28);
            const IVUI::ChartSeriesView stream[] = {
                {"Live", g_chartStream, IM_ARRAYSIZE(g_chartStream), 0.0, 1.0}};
            IVUI::ChartStreamOptions streamOptions;
            streamOptions.maxPoints = IM_ARRAYSIZE(g_chartStream);
            streamOptions.followLatest = true;
            g_charts->RealTime("phase4_chart_stream", stream, streamOptions, options, &th);
        });

        g_ui->layouts().Section("Model Viewer", &g_effectModelViewerOpen, [&] {
            if (EnsurePreviewScene())
            {
                IVUI::ModelViewerOptions options;
                options.size = ImVec2(0.0f, 260.0f * S);
                IVUI::ModelViewer("phase5_model_viewer", g_previewScene,
                                  g_modelViewerState, options);
            }
            else
            {
                ImGui::TextDisabled("CesiumMilkTruck.glb: %s",
                                    g_previewSceneError.empty()
                                        ? "asset unavailable"
                                        : g_previewSceneError.c_str());
            }
        });
    }

    static void RenderConfigTab()
    {
        Heading("Config");
        g_ui->widgets().HotkeyInput("Menu Key", &g_menuKey);
        g_ui->widgets().Combo("界面风格 Style", &g_style,
                    { "Material You", "VAPE", "Neverlose", "ImGui Classic", "iOS Liquid Glass", "Studio Dark", "Studio Light" });
        g_ui->widgets().Combo("Density", &g_density, {"Comfortable", "Compact"});
        g_ui->widgets().Toggle("Reduced Motion", &g_reducedMotion);
        g_ui->widgets().SliderFloat("Motion Speed", &g_motionSpeed, 0.0f, 2.0f, "%.2fx");
        IVUI::Tooltip("切换整套视觉风格 · 切换会平滑形变");
        g_ui->widgets().ColorPicker("Accent Color", &g_accent);
        g_ui->widgets().ColorPicker("Glass Tint", &g_glassTint);
        g_ui->widgets().SliderFloat("Glass Tint Mix", &g_glassTintStrength, 0.0f, 1.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Opacity", &g_glassOpacity, 0.05f, 1.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Refraction", &g_glassRefraction, 0.0f, 8.0f, "%.2f px");
        g_ui->widgets().SliderFloat("Glass Edge Width", &g_glassEdgeWidth, 0.5f, 8.0f, "%.2f px");
        g_ui->widgets().SliderFloat("Glass Rim Light", &g_glassEdgeHighlight, 0.0f, 1.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Thickness", &g_glassThickness, 0.0f, 1.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Shadow", &g_glassShadow, 0.0f, 1.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Shape Power", &g_glassShapePower, 2.0f, 8.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Lens Strength", &g_glassLensStrength, 0.0f, 4.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Lens Base", &g_glassLensBase, 1.01f, 8.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Lens Falloff", &g_glassLensFalloff, 0.1f, 12.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Lens Bias", &g_glassLensBias, 0.0f, 4.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Lens Power", &g_glassLensPower, 0.1f, 4.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glass Noise", &g_glassNoise, 0.0f, 3.0f, "%.2f px");
        g_ui->widgets().SliderFloat("UI Scale", &g_scale, 0.8f, 1.4f, "%.2fx");
        g_ui->widgets().SliderFloat("Roundness", &g_roundMul, 0.0f, 2.0f, "%.2f");
        g_ui->widgets().SliderFloat("Glow", &g_glowMul, 0.0f, 2.5f, "%.2f");
        g_ui->widgets().SliderFloat("Translucency", &g_translucencyMul, 0.3f, 1.2f, "%.2f");
        g_ui->widgets().SliderFloat("Backdrop Blur", &g_backdropBlurSigma, 0.0f, 16.0f, "%.2f px");
        g_ui->layouts().Section("Keybinds (primary + secondary + mode)", &g_cfgKeybinds, [] {
            g_ui->widgets().KeybindRow("Aimbot", &g_kbAim);
            g_ui->widgets().KeybindRow("ESP", &g_kbEsp);
            g_ui->widgets().KeybindRow("Panic", &g_kbPanic);
        });
        g_ui->widgets().Toggle("Streamproof", &g_streamproof);
    }

    static void RenderAddBanner()
    {
        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;
        std::string name;
        for (auto& p : g_panels) if (p.id == g_editTarget) name = p.name;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = th.ControlHeight * S * 0.82f;
        dl->AddRectFilled(c, ImVec2(c.x + w, c.y + h), IVUI::WithAlphaMul(th.Accent, 0.16f), th.RoundingControl * S);
        dl->AddRect(c, ImVec2(c.x + w, c.y + h), th.Accent, th.RoundingControl * S, 0, th.BorderThickness * S);
        const std::string msg = "编辑 " + name + " · 拖或点主菜单功能加入 · Esc 完成";
        const ImVec2 ts = IVUI::CalcTextScaled(msg.c_str(), th.FontSizeLabel);
        IVUI::AddTextScaled(dl, ImVec2(c.x + 10.0f * S, c.y + (h - ts.y) * 0.5f), th.Text, msg.c_str(), th.FontSizeLabel);
        const float bw = 80.0f * S, bh = th.ControlHeight * S * 0.6f;
        ImGui::SetCursorScreenPos(ImVec2(c.x + w - bw - 6.0f * S, c.y + (h - bh) * 0.5f));
        if (g_ui->widgets().Button("完成##adddone", {.size = ImVec2(bw, bh), .prominent = true}))
            g_editTarget.clear();
        ImGui::SetCursorScreenPos(c);
        ImGui::Dummy(ImVec2(w, h + 8.0f * S));
    }

    static void RenderCustomPanels(const ImGuiViewport* /*vp*/, float S)
    {
        for (size_t i = 0; i < g_panels.size(); ++i)
        {
            UserPanel* p = &g_panels[i];
            const bool editMode = (g_editTarget == p->id);
            IVUI::PanelHeader hdr;
            hdr.showEdit = true;
            hdr.showClose = true;
            hdr.editActive = editMode;
            hdr.editBuf = g_renameBuf;
            hdr.editBufSize = (int)sizeof(g_renameBuf);
            // The ID suffix keeps panel state stable when its display name changes.
            const std::string label = p->name + "###" + p->id;
            std::vector<std::string> toRemove;
            std::string dropFeat; int dropIdx = -1;   // drag-add: feature id + insertion index
            int rFrom = -1, rTo = -1;                  // reorder: from index + insertion index

            g_ui->layouts().Panel(label.c_str(), p->pos, ImVec2(260.0f, 520.0f), [&] {
                const IVUI::Theme& th = g_ui->theme().Current();
                const float S = th.Scale;
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if (p->items.empty() && !editMode)
                {
                    const ImVec2 c = ImGui::GetCursorScreenPos();
                    const char* hint = "点铅笔 ✎ 进入编辑后添加功能";
                    IVUI::AddTextScaled(dl, c, th.TextDisabled, hint, th.FontSizeSmall);
                    const ImVec2 ts = IVUI::CalcTextScaled(hint, th.FontSizeSmall);
                    ImGui::Dummy(ImVec2(ImMax(ts.x, 200.0f * S), ts.y + 8.0f * S));
                    return;
                }

                const float x0 = ImGui::GetCursorScreenPos().x;
                const float x1 = x0 + ImGui::GetContentRegionAvail().x;
                const float bodyTop = ImGui::GetCursorScreenPos().y;
                std::vector<float> rowTop, rowBot;
                for (int j = 0; j < (int)p->items.size(); ++j)
                {
                    const float t = ImGui::GetCursorScreenPos().y;
                    if (PanelFeatureRow(p->id, p->items[j], j, editMode))
                        toRemove.push_back(p->items[j].featureId);
                    rowTop.push_back(t);
                    rowBot.push_back(ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y);
                }

                if (editMode)
                {
                    const float pad = th.PanelPadding.x * S;
                    const ImGuiPayload* pl = ImGui::GetDragDropPayload();
                    const bool dragActive = pl && (pl->IsDataType("IVUI_FEAT") || pl->IsDataType("IVUI_REORDER"));

                    if (p->items.empty())
                        IVUI::AddTextScaled(dl, ImGui::GetCursorScreenPos(), th.TextDisabled, "从主菜单拖或点功能加入", th.FontSizeSmall);

                    const float bodyBot = ImGui::GetCursorScreenPos().y;

                    const ImRect body(ImVec2(x0 - pad + 3.0f * S, bodyTop),
                                      ImVec2(x1 + pad - 3.0f * S, ImMax(bodyBot, bodyTop + 12.0f * S)));
                    if (dragActive && ImGui::IsMouseHoveringRect(body.Min, body.Max))
                    {
                        dl->AddRectFilled(body.Min, body.Max, IVUI::WithAlphaMul(th.Accent, 0.10f), th.RoundingControl * S);
                        dl->AddRect(body.Min, body.Max, th.Accent, th.RoundingControl * S, 0, ImMax(2.0f * S, 1.0f));

                        const float my = ImGui::GetMousePos().y;
                        int insertIdx = (int)rowTop.size();
                        for (int j = 0; j < (int)rowTop.size(); ++j)
                            if (my < (rowTop[j] + rowBot[j]) * 0.5f) { insertIdx = j; break; }
                        const float lineY = (insertIdx < (int)rowTop.size()) ? rowTop[insertIdx] : bodyBot - 8.0f * S;
                        dl->AddLine(ImVec2(x0, lineY), ImVec2(x1, lineY), th.Accent, ImMax(3.0f * S, 1.5f));
                        dl->AddCircleFilled(ImVec2(x0, lineY), 4.0f * S, th.Accent);

                        if (ImGui::BeginDragDropTargetCustom(body, ImGui::GetID("panelbody")))
                        {
                            if (const ImGuiPayload* g1 = ImGui::AcceptDragDropPayload("IVUI_FEAT", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                            { dropFeat = std::string((const char*)g1->Data); dropIdx = insertIdx; }
                            else if (const ImGuiPayload* g2 = ImGui::AcceptDragDropPayload("IVUI_REORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                            { rFrom = *(const int*)g2->Data; rTo = insertIdx; }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }
            }, &hdr);

            for (auto& r : toRemove)
                p->items.erase(std::remove_if(p->items.begin(), p->items.end(),
                               [&](const PanelItem& it) { return it.featureId == r; }), p->items.end());

            if (rFrom >= 0 && rTo >= 0 && rFrom < (int)p->items.size())
            {
                PanelItem moved = p->items[rFrom];
                int to = (rFrom < rTo) ? rTo - 1 : rTo; // insertion index shifts after erase
                p->items.erase(p->items.begin() + rFrom);
                if (to < 0) to = 0;
                if (to > (int)p->items.size()) to = (int)p->items.size();
                p->items.insert(p->items.begin() + to, moved);
            }
            if (!dropFeat.empty())
                InsertFeatureToPanel(p->id, dropFeat, dropIdx);

            if (hdr.nameChanged)
            {
                std::string nn = g_renameBuf;
                const size_t s0 = nn.find_first_not_of(" \t");
                const size_t s1 = nn.find_last_not_of(" \t");
                nn = (s0 == std::string::npos) ? std::string() : nn.substr(s0, s1 - s0 + 1);
                if (!nn.empty())
                    p->name = nn;
            }
            if (hdr.editClicked)
                g_editTarget = editMode ? std::string() : p->id;
            if (hdr.closeClicked)
            {
                g_pendingDelete = p->id;
                g_ui->widgets().OpenConfirm("##ivui_delpanel");
            }
        }

        if (g_ui->widgets().ConfirmPopup("##ivui_delpanel", "删除这个面板?", "删除", "取消"))
        {
            if (g_editTarget == g_pendingDelete) g_editTarget.clear();
            g_panels.erase(std::remove_if(g_panels.begin(), g_panels.end(),
                           [](const UserPanel& x) { return x.id == g_pendingDelete; }), g_panels.end());
            g_pendingDelete.clear();
            g_ui->widgets().Notify("面板已移除", "Panel removed");
        }
    }

    void RenderMenu(IVUI::UI& ui, bool* open)
    {
        g_ui = &ui;
        RegisterFeatures();
        ApplyTheme();

        for (int i = 0; i < 5; ++i)
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i)))
            {
                g_style = i;
                IVUI::NotificationOptions options;
                options.type = IVUI::NotificationType::Success;
                options.duration = 8.0f;
                g_ui->widgets().Notify("风格已切换", IVUI::StyleName((IVUI::Style)i), options);
                options.type = IVUI::NotificationType::Info;
                g_ui->widgets().Notify("Notification", "Hello from this style", options);
            }

        const IVUI::Theme& th0 = g_ui->theme().Current();
        const float S0 = th0.Scale;
        const ImGuiViewport* vp = ImGui::GetMainViewport();

        if (!g_seeded)
        {
            CreatePanel(vp, S0);
            g_panels.back().name = "Quick Panel";
            g_panels.back().open = false;
            g_panels.back().items = { { "aimbot.enabled", "" }, { "player.god", "" }, { "weapon.norecoil", "" } };
            g_seeded = true;
        }

        if (g_statusHudEnabled)
        {
            std::vector<const char*> chips;
            if (g_aimEnabled) chips.push_back("Aimbot");
            if (g_espBox || g_espName || g_espDist || g_espHealth) chips.push_back("ESP");
            if (g_noRecoil) chips.push_back("No Recoil");
            if (g_godmode) chips.push_back("God Mode");
            if (g_infAmmo) chips.push_back("Inf Ammo");
            if (g_speedhack) chips.push_back("Speed Hack");
            if (!chips.empty())
            {
                g_ui->layouts().StatusList(ImVec2(vp->Pos.x + vp->Size.x - 16.0f * S0,
                                                  vp->Pos.y + 16.0f * S0), chips.data(), (int)chips.size());
            }
        }

        const float visTarget = *open ? 1.0f : 0.0f;
        const float dt = ImClamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        const float menuResponse = IVUI::MotionResponse(14.0f);
        g_menuVis += (visTarget - g_menuVis) * menuResponse;
        if (ImFabs(g_menuVis - visTarget) < 0.003f) g_menuVis = visTarget;
        if (g_menuVis < 0.003f)
            return;

        if (!g_editTarget.empty() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            g_editTarget.clear();

        if (g_scrimAlpha > 0.001f)
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                IVUI::WithAlphaMul(th0.Overlay, g_scrimAlpha * th0.BackdropAlpha * g_menuVis));

        g_ui->theme().Push(IVUI::ScaleThemeAlpha(g_ui->theme().Current(), g_menuVis));

        const IVUI::Theme& th = g_ui->theme().Current();
        const float S = th.Scale;
        const ImVec2 menuSize(ImMin(1100.0f * S, vp->WorkSize.x),
                              ImMin(760.0f * S, vp->WorkSize.y));
        ImGui::SetNextWindowSize(menuSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - menuSize.x) * 0.5f,
                                       vp->WorkPos.y + (vp->WorkSize.y - menuSize.y) * 0.5f), ImGuiCond_Once);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(ImMin(300.0f * S, vp->WorkSize.x), ImMin(240.0f * S, vp->WorkSize.y)),
            vp->WorkSize);
        ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground;
        if (ImGui::Begin("##ivui_main_menu", nullptr, flags))
        {
            IVUI::WorkbenchOptions workbench;
            workbench.title = "ImGuiVectorUI";
            workbench.status = g_reducedMotion ? "Ready  |  Reduced motion" : "Ready";
            workbench.transparentSurface = true;

            const ImVec2 mainPos = ImGui::GetWindowPos();
            const ImVec2 mainSize = ImGui::GetWindowSize();
            const float mainRounding = th.RoundingPanel * S;
            if (g_ui->effects().Capabilities() != IVUI::EffectCapabilities::None)
            {
                IVUI::BlurEffectParams blur;
                blur.bounds.min = {mainPos.x, mainPos.y};
                blur.bounds.max = {mainPos.x + mainSize.x, mainPos.y + mainSize.y};
                blur.sigma = th.BackdropBlurSigma;
                blur.rounding = mainRounding;
                blur.opacity = g_menuVis;
                g_ui->effects().Submit(IVUI::EffectRequest::BackdropBlur(blur));

                const ImVec4 tint = ImGui::ColorConvertU32ToFloat4(th.GlassTint);
                const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(th.Accent);
                IVUI::GlassEffectParams glass;
                glass.bounds = blur.bounds;
                glass.rounding = mainRounding;
                glass.opacity = th.GlassOpacity * g_menuVis;
                glass.tint = {tint.x, tint.y, tint.z, tint.w};
                glass.tintStrength = th.GlassTintStrength;
                glass.accent = {accent.x, accent.y, accent.z, accent.w};
                glass.blurSigma = th.BackdropBlurSigma;
                glass.refraction = th.GlassRefraction * S;
                glass.edgeWidth = th.GlassEdgeWidth * S;
                glass.edgeHighlight = th.GlassEdgeHighlight;
                glass.thickness = th.GlassThickness;
                glass.shapePower = th.GlassShapePower;
                glass.lensStrength = th.GlassLensStrength;
                glass.lensBase = th.GlassLensBase;
                glass.lensFalloff = th.GlassLensFalloff;
                glass.lensBias = th.GlassLensBias;
                glass.lensPower = th.GlassLensPower;
                glass.noise = th.GlassNoise * S;
                g_ui->effects().Submit(IVUI::EffectRequest::Glass(glass));
            }

            g_ui->layouts().Workbench("studio", workbench, [&](bool compact) {
                const int effectsTab = static_cast<int>(g_categories.size());
                auto name = [&](int index) -> const char* {
                    if (index < effectsTab) return g_categories[index].name.c_str();
                    if (index == effectsTab) return "Components";
                    if (index == effectsTab + 1) return "Layouts";
                    return "Appearance";
                };
                if (compact)
                {
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##page", name(g_tab)))
                    {
                        for (int index = 0; index < effectsTab + 3; ++index)
                        {
                            if (ImGui::Selectable(name(index), g_tab == index)) g_tab = index;
                            if (g_tab == index) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    g_ui->layouts().Tab("Layouts", IVUI::IconType::Sliders, effectsTab + 1, &g_tab);
                    g_ui->layouts().Tab("Components", IVUI::IconType::Star, effectsTab, &g_tab);
                    g_ui->layouts().Tab("Appearance", IVUI::IconType::Gear, effectsTab + 2, &g_tab);
                    g_ui->layouts().Separator();
                    for (int index = 0; index < effectsTab; ++index)
                        g_ui->layouts().Tab(name(index), g_categories[index].icon, index, &g_tab);
                }
            }, [&] {
                const int effectsTab = static_cast<int>(g_categories.size());
                const float fade = g_ui->motion().Transition(ImGui::GetID("page-motion"), g_tab, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * fade);
                g_ui->theme().Push(IVUI::ScaleThemeAlpha(g_ui->theme().Current(), fade));
                ImGui::PushID(g_tab);
                if (g_tab < effectsTab)
                {
                    if (g_ui->widgets().Button("New Panel"))
                        CreatePanel(vp, S);
                    if (!g_editTarget.empty()) RenderAddBanner();
                    RenderCategoryContent(g_categories[g_tab]);
                }
                else if (g_tab == effectsTab)
                    RenderEffectsPreviewTab();
                else if (g_tab == effectsTab + 1)
                    g_layoutGallery.Render(*g_ui);
                else
                    RenderConfigTab();
                ImGui::PopID();
                g_ui->theme().Pop();
                ImGui::PopStyleVar();
            });
        }
        ImGui::End();
        ImGui::PopStyleVar();

        RenderCustomPanels(vp, S);

        g_ui->theme().Pop();
    }
}
