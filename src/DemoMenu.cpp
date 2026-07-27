/** @file DemoMenu.cpp @brief Interactive ImGuiVectorUI demonstration menu. */
#include "DemoMenu.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiVectorUI/ImGuiVectorUI.h"

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace Demo
{
    static int   g_tab = 0;
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
    static bool  g_nightMode = false, g_statusHudEnabled = true;
    static float g_bloom = 1.0f;
    static float g_bloomThreshold = 0.08f;
    static float g_bloomSaturation = 0.04f;
    static float g_bloomSigma = 3.0f;
    static float g_backdropBlurSigma = 16.0f;

    static float g_scrimAlpha = 0.32f;

    static int    g_menuKey = ImGuiKey_Insert;
    static int    g_style = 0;                    // IVUI::Style (Material You / VAPE / Neverlose / ImGui / iOS)
    static float  g_scale = 1.0f, g_roundMul = 1.0f;
    static float  g_glowMul = 1.0f, g_translucencyMul = 1.0f; // free-adjust multipliers
    static ImVec4 g_accent = ImVec4(1.0f, 180.0f / 255.0f, 169.0f / 255.0f, 1.0f);
    static ImVec4 g_glassTint = ImVec4(0.11f, 0.12f, 0.16f, 1.0f);
    static float  g_glassTintStrength = 0.16f, g_glassOpacity = 0.82f;
    static bool   g_cfgKeybinds = true, g_streamproof = false;
    static IVUI::Keybind g_kbAim{ ImGuiKey_C }, g_kbEsp{ ImGuiKey_E }, g_kbPanic{ ImGuiKey_Delete };

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
        const IVUI::Theme& th = IVUI::GetTheme();
        const float S = th.Scale;
        struct Ent { const char* name; float dist; int hp; };
        static Ent ents[] = {
            { "Bandit", 42.5f, 80 }, { "Skag", 12.0f, 100 }, { "Psycho", 88.3f, 45 },
            { "Loader", 150.0f, 100 }, { "Badass", 30.0f, 100 }, { "Varkid", 64.0f, 60 } };
        const int N = (int)(sizeof(ents) / sizeof(ents[0]));
        if (IVUI::BeginTable("##ents", 3, ImVec2(0.0f, 140.0f * S)))
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
            IVUI::EndTable();
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

        add("aimbot", "", "enabled", "Enabled", [](const char* l) { IVUI::Toggle(l, &g_aimEnabled); });
        add("aimbot", "", "fov", "FOV", [](const char* l) { IVUI::SliderFloat(l, &g_fov, 1.0f, 180.0f, "%.0f"); });
        add("aimbot", "", "smoothing", "Smoothing", [](const char* l) { IVUI::SliderFloat(l, &g_smooth, 1.0f, 20.0f, "%.1f"); });
        add("aimbot", "", "mode", "Mode", [](const char* l) { IVUI::SliderLabeled(l, &g_aimMode, { "Off", "Legit", "Rage" }); });
        add("aimbot", "Advanced", "vischeck", "Visibility Check", [](const char* l) { IVUI::Toggle(l, &g_aimVisCheck); });
        add("aimbot", "Advanced", "bone", "Target Bone", [](const char* l) { IVUI::Combo(l, &g_aimBone, { "Head", "Neck", "Chest", "Pelvis" }); });

        add("esp", "", "boxes", "Boxes", [](const char* l) {
            IVUI::ComboMulti(l, { { "2D Box", &g_espBox }, { "Name", &g_espName }, { "Distance", &g_espDist }, { "Health Bar", &g_espHealth } });
        });
        add("esp", "", "distance", "Max Distance", [](const char* l) { IVUI::SliderFloat(l, &g_espDistance, 10.0f, 1000.0f, "%.0f m"); });
        add("esp", "", "color", "Box Color", [](const char* l) { IVUI::ColorPicker(l, &g_espColor); });
        add("esp", "", "entities", "Entity List", [](const char*) { DrawEntityTable(); });

        add("weapon", "", "mode", "Fire Mode", [](const char* l) { IVUI::Combo(l, &g_weaponMode, { "Legit", "Rage", "Hybrid" }); });
        add("weapon", "", "firerate", "Fire Rate", [](const char* l) { IVUI::SliderSegments(l, &g_fireRate, 5, "%d"); });
        add("weapon", "", "norecoil", "No Recoil", [](const char* l) { IVUI::Toggle(l, &g_noRecoil); });
        add("weapon", "", "apply", "Apply Loadout", [](const char* l) { if (IVUI::Button(l, ImVec2(0, 0), true)) g_weaponMode = (g_weaponMode + 1) % 3; });

        add("player", "", "god", "God Mode", [](const char* l) { IVUI::Toggle(l, &g_godmode); });
        add("player", "", "ammo", "Infinite Ammo", [](const char* l) { IVUI::Toggle(l, &g_infAmmo); });
        add("player", "", "speed", "Speed Hack", [](const char* l) { IVUI::Toggle(l, &g_speedhack); });
        add("player", "", "speedmult", "Speed Multiplier", [](const char* l) { IVUI::SliderFloat(l, &g_speedMult, 1.0f, 5.0f, "%.2fx"); });

        add("visual", "", "quality", "Quality", [](const char* l) { IVUI::SliderLabeled(l, &g_quality, { "Low", "Medium", "High", "Ultra" }); });
        add("visual", "", "bloom", "Bloom Strength", [](const char* l) { IVUI::SliderFloat(l, &g_bloom, 0.0f, 2.0f, "%.2f"); });
        add("visual", "Bloom", "bloom_threshold", "Bloom Threshold", [](const char* l) { IVUI::SliderFloat(l, &g_bloomThreshold, 0.0f, 1.0f, "%.2f"); });
        add("visual", "Bloom", "bloom_saturation", "Bloom Saturation", [](const char* l) { IVUI::SliderFloat(l, &g_bloomSaturation, 0.0f, 0.5f, "%.2f"); });
        add("visual", "Bloom", "bloom_sigma", "Bloom Sigma", [](const char* l) { IVUI::SliderFloat(l, &g_bloomSigma, 0.5f, 3.0f, "%.2f"); });
        add("visual", "", "night", "Night Mode", [](const char* l) { IVUI::Toggle(l, &g_nightMode); });
        add("visual", "", "feature_hud", "Feature HUD", [](const char* l) { IVUI::Toggle(l, &g_statusHudEnabled); });

        add("visual", "Glass Effect", "glass_scrim", "Backdrop Dim", [](const char* l) { IVUI::SliderFloat(l, &g_scrimAlpha, 0.0f, 0.85f, "%.2f"); });

        add("visual", "Notifications", "notify", "Fire Toast", [](const char*) {
            if (IVUI::Button("Info", ImVec2(-1.0f, 0.0f), false))
                IVUI::Toast(IVUI::ToastType::Info, "提示 Info", "这是一条信息通知");
            if (IVUI::Button("Success", ImVec2(-1.0f, 0.0f), true))
                IVUI::Toast(IVUI::ToastType::Success, "成功 Success", "操作已完成");
            if (IVUI::Button("Warning", ImVec2(-1.0f, 0.0f), false))
                IVUI::Toast(IVUI::ToastType::Warning, "警告 Warning", "请注意这个状态");
            if (IVUI::Button("Error", ImVec2(-1.0f, 0.0f), false))
                IVUI::Toast(IVUI::ToastType::Error, "错误 Error", "发生了一个错误");
        });
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
        const IVUI::Theme& th = IVUI::GetTheme();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c = ImGui::GetCursorScreenPos();
        IVUI::AddTextScaled(dl, c, th.Accent, text, th.FontSizeTitle, IVUI::GetTitleFont());
        const ImVec2 s = IVUI::CalcTextScaled(text, th.FontSizeTitle, IVUI::GetTitleFont());
        ImGui::Dummy(ImVec2(s.x, s.y + 8.0f * th.Scale));
    }

    static void ApplyTheme()
    {
        IVUI::Theme base = IVUI::GetStyle((IVUI::Style)g_style);
        static int prevStyle = -1;
        if (g_style != prevStyle)
        {
            g_accent = ImGui::ColorConvertU32ToFloat4(base.Accent);
            g_glassTint = ImGui::ColorConvertU32ToFloat4(base.GlassTint);
            g_glassTintStrength = base.GlassTintStrength;
            g_glassOpacity = base.GlassOpacity;
            g_bloomThreshold = base.BloomThreshold;
            g_bloomSaturation = base.BloomSaturation;
            g_bloomSigma = base.BloomSigma;
            g_backdropBlurSigma = base.BackdropBlurSigma;
            prevStyle = g_style;
        }
        IVUI::SetAccent(base, ImGui::ColorConvertFloat4ToU32(g_accent));
        base.Scale = g_scale * IVUI::GetContentScale();
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
        IVUI::SetTargetTheme(base);
    }

    static void MainFeatureRow(const Feature& f)
    {
        const IVUI::Theme& th = IVUI::GetTheme();
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
        IVUI::BeginFeatureDragSource(f.id.c_str(), f.name.c_str());
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
        const IVUI::Theme& th = IVUI::GetTheme();
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
            if (IVUI::Button("+ Add whole category"))
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
            IVUI::Section(grp.c_str(), &g_grpOpen[slot], [&] {
                for (auto& f : g_features)
                    if (f.category == cat.id && f.group == grp)
                        MainFeatureRow(f);
            });
        }
    }

    static void RenderConfigTab()
    {
        Heading("Config");
        IVUI::HotkeyInput("Menu Key", &g_menuKey);
        IVUI::Combo("界面风格 Style", &g_style,
                    { "Material You", "VAPE", "Neverlose", "ImGui Classic", "iOS Liquid Glass" });
        IVUI::Tooltip("切换整套视觉风格 · 切换会平滑形变");
        IVUI::ColorPicker("Accent Color", &g_accent);
        IVUI::ColorPicker("Glass Tint", &g_glassTint);
        IVUI::SliderFloat("Glass Tint Mix", &g_glassTintStrength, 0.0f, 1.0f, "%.2f");
        IVUI::SliderFloat("Glass Opacity", &g_glassOpacity, 0.05f, 1.0f, "%.2f");
        IVUI::SliderFloat("UI Scale", &g_scale, 0.8f, 1.4f, "%.2fx");
        IVUI::SliderFloat("Roundness", &g_roundMul, 0.0f, 2.0f, "%.2f");
        IVUI::SliderFloat("Glow", &g_glowMul, 0.0f, 2.5f, "%.2f");
        IVUI::SliderFloat("Translucency", &g_translucencyMul, 0.3f, 1.2f, "%.2f");
        IVUI::SliderFloat("Backdrop Blur", &g_backdropBlurSigma, 0.0f, 16.0f, "%.2f px");
        IVUI::Section("Keybinds (primary + secondary + mode)", &g_cfgKeybinds, [] {
            IVUI::KeybindRow("Aimbot", &g_kbAim);
            IVUI::KeybindRow("ESP", &g_kbEsp);
            IVUI::KeybindRow("Panic", &g_kbPanic);
        });
        IVUI::Toggle("Streamproof", &g_streamproof);
        if (IVUI::Button("Save Config")) {}
        ImGui::SameLine();
        if (IVUI::Button("Load Config")) {}
    }

    static void RenderAddBanner()
    {
        const IVUI::Theme& th = IVUI::GetTheme();
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
        if (IVUI::Button("完成##adddone", ImVec2(bw, bh), true))
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

            IVUI::Panel(label.c_str(), p->pos, ImVec2(260.0f, 520.0f), [&] {
                const IVUI::Theme& th = IVUI::GetTheme();
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
                IVUI::OpenConfirm("##ivui_delpanel");
            }
        }

        if (IVUI::ConfirmPopup("##ivui_delpanel", "删除这个面板?", "删除", "取消"))
        {
            if (g_editTarget == g_pendingDelete) g_editTarget.clear();
            g_panels.erase(std::remove_if(g_panels.begin(), g_panels.end(),
                           [](const UserPanel& x) { return x.id == g_pendingDelete; }), g_panels.end());
            g_pendingDelete.clear();
            IVUI::Toast(IVUI::ToastType::Info, "面板已移除", "Panel removed");
        }
    }

    void RenderMenu(bool* open)
    {
        RegisterFeatures();
        ApplyTheme();

        for (int i = 0; i < 5; ++i)
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i)))
            {
                g_style = i;
                IVUI::Toast(IVUI::ToastType::Success, "风格已切换", IVUI::StyleName((IVUI::Style)i), 8.0f);
                IVUI::Toast(IVUI::ToastType::Info, "Notification", "Hello from this style", 8.0f);
            }

        const IVUI::Theme& th0 = IVUI::GetTheme();
        const float S0 = th0.Scale;
        const ImGuiViewport* vp = ImGui::GetMainViewport();

        if (!g_seeded)
        {
            CreatePanel(vp, S0);
            g_panels.back().name = "Quick Panel";
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
                IVUI::StatusList(ImVec2(vp->Pos.x + vp->Size.x - 16.0f * S0,
                                        vp->Pos.y + 16.0f * S0), chips.data(), (int)chips.size());
            }
        }

        IVUI::RenderToasts();

        const float visTarget = *open ? 1.0f : 0.0f;
        g_menuVis += (visTarget - g_menuVis) * ImClamp(IVUI::DeltaTime() * 10.0f, 0.0f, 1.0f);
        if (ImFabs(g_menuVis - visTarget) < 0.003f) g_menuVis = visTarget;
        if (g_menuVis < 0.003f)
            return;

        if (!g_editTarget.empty() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            g_editTarget.clear();

        if (g_scrimAlpha > 0.001f)
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                IVUI::WithAlphaMul(th0.Overlay, g_scrimAlpha * th0.BackdropAlpha * g_menuVis));

        IVUI::PushTheme(IVUI::ScaleThemeAlpha(IVUI::GetTheme(), g_menuVis));

        const IVUI::Theme& th = IVUI::GetTheme();
        const float S = th.Scale;
        const float pad = th.PanelPadding.x * S;
        const float titleH = th.ControlHeight * S;
        const float sidebarW = 190.0f * S;

        // FirstUseEver preserves user-driven resizing after the initial layout.
        const ImVec2 menuSize = ImVec2(940.0f * S, 580.0f * S);
        ImGui::SetNextWindowSize(menuSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + (vp->Size.x - menuSize.x) * 0.5f,
                                       vp->Pos.y + (vp->Size.y - menuSize.y) * 0.5f), ImGuiCond_Once);

        // A title-less window needs edge resizing enabled explicitly.
        ImGui::GetIO().ConfigWindowsResizeFromEdges = true;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##ivui_main_menu", open, flags))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wpos = ImGui::GetWindowPos();
            const ImVec2 wsize = ImGui::GetWindowSize();

            // Submit the backdrop before ImGui so it remains beneath menu content.
            const float mround = th.RoundingPanel * S;
            {
                IVUI::GlassRect g;
                g.min = wpos; g.max = wpos + wsize;
                g.rounding = mround;
                g.opacity = th.GlassOpacity * g_menuVis;
                g.tint = th.GlassTint; g.tintStrength = th.GlassTintStrength;
                g.accent = th.Accent; g.glow = th.Glow;
                g.glowStrength = th.PanelGlow * g_menuVis; g.glowSpread = th.PanelGlowSpread * S;
                g.shadowStrength = th.PanelShadow * g_menuVis; g.shadowSpread = th.PanelShadowSpread * S;
                IVUI::SubmitGlass(g);
            }
            if (th.BorderThickness > 0.01f)
                dl->AddRect(wpos, wpos + wsize, th.Border, mround, 0, th.BorderThickness * S);

            const ImVec2 titleEnd = ImVec2(wpos.x + wsize.x, wpos.y + titleH);
            dl->AddRectFilled(wpos, titleEnd, IVUI::WithAlphaMul(th.Surface1, th.PanelBodyAlpha * 0.10f * g_menuVis), mround, ImDrawFlags_RoundCornersTop);
            IVUI::Sheen(dl, wpos, titleEnd, mround, th.Sheen);
            dl->AddLine(ImVec2(wpos.x + pad, titleEnd.y), ImVec2(titleEnd.x - pad, titleEnd.y), th.Accent, ImMax(2.0f * S, 1.0f));
            const char* title = "VECTOR MENU";
            const ImVec2 tsz = IVUI::CalcTextScaled(title, th.FontSizeTitle, IVUI::GetTitleFont());
            IVUI::AddTextScaled(dl, ImVec2(wpos.x + pad, wpos.y + (titleH - tsz.y) * 0.5f), th.Text, title, th.FontSizeTitle, IVUI::GetTitleFont());
            const char* sub = "注入悬浮菜单 · メニュー · Меню · overlay";
            const ImVec2 subSz = IVUI::CalcTextScaled(sub, th.FontSizeSmall);
            IVUI::AddTextScaled(dl, ImVec2(titleEnd.x - subSz.x - pad, wpos.y + (titleH - subSz.y) * 0.5f), th.TextDisabled, sub, th.FontSizeSmall);

            ImGui::SetCursorPos(ImVec2(pad, titleH + pad));
            ImGui::BeginChild("##sidebar", ImVec2(sidebarW, -pad), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f * S));
                for (int i = 0; i < (int)g_categories.size(); ++i)
                    IVUI::Tab(g_categories[i].name.c_str(), g_categories[i].icon, i, &g_tab);
                IVUI::Tab("Config", IVUI::IconType::Gear, (int)g_categories.size(), &g_tab);
                ImGui::PopStyleVar();

                ImGui::Dummy(ImVec2(0.0f, 10.0f * S));
                if (IVUI::Button("+ New Panel", ImVec2(-1.0f, 0.0f), true))
                {
                    CreatePanel(vp, S);
                    IVUI::Toast(IVUI::ToastType::Success, "面板已创建", g_panels.back().name.c_str());
                }
                IVUI::Tooltip("新建一个自定义面板 · 双击面板标题可重命名");
            }
            ImGui::EndChild();

            ImGui::SameLine(0.0f, pad);
            ImGui::BeginChild("##content", ImVec2(-pad, -pad), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * S, 12.0f * S));
                if (!g_editTarget.empty())
                    RenderAddBanner();
                // Complete the fade-out before switching content to avoid mixed pages.
                static int shownTab = -1;
                static float tabFade = 1.0f;
                if (shownTab < 0)
                    shownTab = g_tab;
                if (shownTab != g_tab)
                {
                    tabFade = ImMax(0.0f, tabFade - IVUI::DeltaTime() * 8.0f);
                    if (tabFade <= 0.001f)
                        shownTab = g_tab;
                }
                else
                {
                    tabFade = ImMin(1.0f, tabFade + IVUI::DeltaTime() * 8.0f);
                }
                const float tp = IVUI::EaseValue(0.0f, 1.0f, tabFade, IVUI::EASE_IN_OUT_SINE);
                IVUI::PushTheme(IVUI::ScaleThemeAlpha(IVUI::GetTheme(), tp));
                if (shownTab < (int)g_categories.size())
                    RenderCategoryContent(g_categories[shownTab]);
                else
                    RenderConfigTab();
                IVUI::PopTheme();
                ImGui::PopStyleVar();
                IVUI::OverlayScrollbar();
            }
            ImGui::EndChild();
        }
        ImGui::End();

        RenderCustomPanels(vp, S);

        IVUI::PopTheme();
    }
}
