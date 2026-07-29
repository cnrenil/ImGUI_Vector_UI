#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Widgets/Toast.h"

#include <algorithm>

namespace IVUI
{
    static thread_local Context* g_currentContext = nullptr;

    Context* CreateContext()
    {
        if (ImGui::GetCurrentContext() == nullptr || g_currentContext != nullptr)
            return g_currentContext;
        Context* context = new Context();
        context->themeStack.reserve(16);
        context->themeStack.push_back(ThemeInjectionPeach()); // default palette
        context->themeTarget = context->themeLive = ThemeInjectionPeach();
        context->themeInit = true;
        g_currentContext = context;
        return context;
    }

    void DestroyContext(Context* context)
    {
        if (context == nullptr)
            context = g_currentContext;
        if (context == nullptr)
            return;
        context->toasts.clear();
        context->anim.Clear();
        if (g_currentContext == context)
            g_currentContext = nullptr;
        delete context;
    }

    Context* SetCurrentContext(Context* context)
    {
        Context* previous = g_currentContext;
        g_currentContext = context;
        return previous;
    }

    Context* GetContext()
    {
        return g_currentContext;
    }

    void SetFonts(ImFont* regular, ImFont* title)
    {
        Context* context = GetContext();
        if (context == nullptr)
            return;
        context->fontRegular = regular;
        context->fontTitle = title;
    }

    bool WantCaptureHotkey()
    {
        Context* context = GetContext();
        return context != nullptr && context->wantCaptureHotkey;
    }

    void SetContentScale(float scale)
    {
        Context* context = GetContext();
        if (context != nullptr && scale > 0.0f)
            context->contentScale = scale;
    }

    float GetContentScale()
    {
        Context* context = GetContext();
        return context != nullptr ? context->contentScale : 1.0f;
    }

    void NewFrame()
    {
        Context* context = GetContext();
        if (context == nullptr)
            return;
        // Clamp stalls to keep animation integration stable.
        context->deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        context->frameCount++;
        context->wantCaptureHotkey = false; // widgets set this again during the frame
        AdvanceThemeMorph(context->deltaTime); // ease live theme -> target, publish to stack base

        // Reclaim state only after traversal has completed for several frames.
        if ((context->frameCount % 240) == 0)
            context->anim.GarbageCollect(context->frameCount, 600);
    }
}
