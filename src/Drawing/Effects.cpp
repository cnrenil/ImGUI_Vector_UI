#include "ImGuiVectorUI/Drawing/Effects.h"
#include "ImGuiVectorUI/Core/Context.h"

namespace IVUI
{
    static GlassStyle g_glassStyle;

    void SetGlassStyle(const GlassStyle& s) { g_glassStyle = s; }
    const GlassStyle& GetGlassStyle() { return g_glassStyle; }

    void SubmitGlass(const GlassRect& r)
    {
        Context* ctx = GetContext();
        if (ctx == nullptr)
            return;
        ctx->glassQueue.push_back(r);
    }

    const GlassRect* CollectGlass(int* outCount)
    {
        Context* ctx = GetContext();
        const int n = (ctx != nullptr) ? (int)ctx->glassQueue.size() : 0;
        if (outCount)
            *outCount = n;
        return (ctx != nullptr && n > 0) ? ctx->glassQueue.data() : nullptr;
    }
}
