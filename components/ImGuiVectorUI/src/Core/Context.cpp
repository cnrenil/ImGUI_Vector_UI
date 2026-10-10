#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Widgets/Notification.h"

#include <algorithm>
#include <cmath>

namespace IVUI
{
    static thread_local Context* g_currentContext = nullptr;

    Context* CreateContext()
    {
        // A library context belongs to one ImGui context, but multiple UI
        // instances may be created serially while the thread-local current
        // context still points at a previous instance.  Returning that old
        // pointer aliases ownership and lets a later UI destroy it.
        if (ImGui::GetCurrentContext() == nullptr)
            return nullptr;
        Context* context = new Context();
        context->themeStack.reserve(16);
        context->themeStack.push_back(MakeTheme());
        context->themeTarget = context->themeLive = context->themeStack.front();
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
        context->notifications.clear();
        context->itemEffectData.clear();
        context->lifetime.reset();
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
        if (context != nullptr && std::isfinite(scale) && scale > 0.0f)
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
        const float dt = ImGui::GetIO().DeltaTime;
        context->deltaTime = std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.1f) : 0.0f;
        if (!context->motion.reducedMotion)
            context->motionTime += context->deltaTime * context->motion.speed;
        context->frameCount++;
        context->wantCaptureHotkey = false; // widgets set this again during the frame
        context->itemEffectData.clear();
        context->activeItemEffect = nullptr;
        AdvanceThemeMorph(context->deltaTime * context->motion.speed);

        // Reclaim state only after traversal has completed for several frames.
        if ((context->frameCount % 240) == 0)
            context->anim.GarbageCollect(context->frameCount, 600);
    }
}
