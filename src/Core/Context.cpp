#include "ImGuiVectorUI/Core/Context.h"

#include "imgui_internal.h" // ImClamp

namespace IVUI
{
    static Context* g_ctx = nullptr;

    void CreateContext()
    {
        if (g_ctx != nullptr)
            return;
        g_ctx = IM_NEW(Context)();
        g_ctx->themeStack.reserve(16); // stable addresses: PushTheme won't reallocate
        g_ctx->themeStack.push_back(ThemeInjectionPeach()); // default palette
        g_ctx->themeTarget = g_ctx->themeLive = ThemeInjectionPeach();
        g_ctx->themeInit = true;
    }

    void DestroyContext()
    {
        if (g_ctx == nullptr)
            return;
        g_ctx->anim.Clear();
        IM_DELETE(g_ctx);
        g_ctx = nullptr;
    }

    Context* GetContext()
    {
        return g_ctx;
    }

    void SetFonts(ImFont* regular, ImFont* title)
    {
        if (g_ctx == nullptr)
            return;
        g_ctx->fontRegular = regular;
        g_ctx->fontTitle = title;
    }

    bool WantCaptureHotkey()
    {
        return g_ctx != nullptr && g_ctx->wantCaptureHotkey;
    }

    void SetContentScale(float scale)
    {
        if (g_ctx != nullptr && scale > 0.0f)
            g_ctx->contentScale = scale;
    }

    float GetContentScale()
    {
        return g_ctx != nullptr ? g_ctx->contentScale : 1.0f;
    }

    void NewFrame()
    {
        if (g_ctx == nullptr)
            return;
        // Clamp stalls to keep animation integration stable.
        g_ctx->deltaTime = ImClamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        g_ctx->frameCount++;
        g_ctx->wantCaptureHotkey = false; // widgets set this again during the frame
        AdvanceThemeMorph(g_ctx->deltaTime); // ease live theme -> target, publish to stack base

        // Reclaim state only after traversal has completed for several frames.
        if ((g_ctx->frameCount % 240) == 0)
            g_ctx->anim.GarbageCollect(g_ctx->frameCount, 600);
    }
}
