#include "ImGuiVectorUI/Core/DragDrop.h"
#include "ImGuiVectorUI/Theme/Theme.h"
#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Core/Context.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string.h>

namespace IVUI
{
    static constexpr const char* kPayload = "IVUI_FEAT";

    void DrawDragGhost(const char* name)
    {
        const Theme& th = GetTheme();
        const float S = th.Scale;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 ts = CalcTextScaled(name, th.FontSizeLabel);
        const ImVec2 padv(12.0f * S, 7.0f * S);
        const ImVec2 sz(ts.x + padv.x * 2.0f, ts.y + padv.y * 2.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y), th.Accent, th.RoundingControl * S);
        AddTextScaled(dl, ImVec2(pos.x + padv.x, pos.y + padv.y), th.OnAccent, name, th.FontSizeLabel);
        ImGui::Dummy(sz);
    }

    bool BeginFeatureDragSource(const char* featureId, const char* displayName)
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload(kPayload, featureId, strlen(featureId) + 1);
            DrawDragGhost(displayName ? displayName : featureId);
            ImGui::EndDragDropSource();
            return true;
        }
        return false;
    }

    static const char* CopyPayload(const ImGuiPayload* p)
    {
        Context* context = GetContext();
        if (!p || !context)
            return nullptr;
        auto& buffer = context->dropBuffer;
        size_t n = (size_t)p->DataSize;
        if (n >= buffer.size())
            n = buffer.size() - 1;
        memcpy(buffer.data(), p->Data, n);
        buffer[n] = 0;
        return buffer.data();
    }

    const char* AcceptFeatureDrop()
    {
        const char* result = nullptr;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayload))
                result = CopyPayload(p);
            ImGui::EndDragDropTarget();
        }
        return result;
    }

    const char* AcceptFeatureDropOnRect(const void* imguiId, float x0, float y0, float x1, float y1)
    {
        const char* result = nullptr;
        ImRect rect(ImVec2(x0, y0), ImVec2(x1, y1));
        if (ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID(imguiId)))
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayload))
                result = CopyPayload(p);
            ImGui::EndDragDropTarget();
        }
        return result;
    }

    bool IsFeatureDragActive()
    {
        const ImGuiPayload* p = ImGui::GetDragDropPayload();
        return p && p->IsDataType(kPayload);
    }
}
