#include "ImGuiVectorUI/Drawing/Effects.h"
#include "Drawing/ItemEffectsInternal.h"
#include "ImGuiVectorUI/Core/Context.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <stdexcept>
#include <cstring>
#include <type_traits>
#include <unordered_map>

namespace IVUI
{
    struct EffectRegistry::Impl
    {
        struct Entry { CustomEffectCallback callback = nullptr; void* userData = nullptr; };
        IEffectRenderer* backend = nullptr;
        std::unordered_map<std::uint32_t, Entry> handlers;
    };

    EffectRegistry::EffectRegistry(IEffectRenderer* backend) : impl_(std::make_unique<Impl>())
    {
        impl_->backend = backend;
    }
    EffectRegistry::~EffectRegistry() = default;
    EffectRegistry::EffectRegistry(EffectRegistry&&) noexcept = default;
    EffectRegistry& EffectRegistry::operator=(EffectRegistry&&) noexcept = default;
    bool EffectRegistry::Register(EffectId id, CustomEffectCallback callback, void* userData)
    {
        if (static_cast<std::uint32_t>(id) < static_cast<std::uint32_t>(EffectId::CustomFirst) || !callback)
            return false;
        impl_->handlers[static_cast<std::uint32_t>(id)] = {callback, userData};
        return true;
    }
    bool EffectRegistry::Unregister(EffectId id)
    {
        return impl_->handlers.erase(static_cast<std::uint32_t>(id)) != 0;
    }
    void EffectRegistry::SetBackend(IEffectRenderer* backend) noexcept { impl_->backend = backend; }
    EffectCapabilities EffectRegistry::GetCapabilities() const noexcept
    {
        return impl_->backend ? impl_->backend->GetCapabilities() : EffectCapabilities::None;
    }
    void EffectRegistry::RenderEffect(const EffectRequest& request) noexcept
    {
        const auto found = impl_->handlers.find(static_cast<std::uint32_t>(request.id));
        if (found != impl_->handlers.end() && impl_->backend)
            found->second.callback(found->second.userData, request, *impl_->backend);
        else if (impl_->backend)
            impl_->backend->RenderEffect(request);
    }
    void EffectRegistry::RenderItemEffects(const ItemEffectRequest& request,
                                           const ItemEffectDrawData& drawData) noexcept
    {
        if (impl_->backend)
            impl_->backend->RenderItemEffects(request, drawData);
    }
    IEffectRenderer* EffectRegistry::GetBackendRenderer() noexcept
    {
        return impl_->backend ? impl_->backend->GetBackendRenderer() : nullptr;
    }

    namespace
    {
        struct EffectDrawCommand
        {
            IEffectRenderer* renderer = nullptr;
            EffectRequest request{};
        };

        static_assert(std::is_trivially_copyable_v<EffectDrawCommand>);

        struct PendingItemEffect
        {
            IEffectRenderer* renderer = nullptr;
            ImDrawList* drawList = nullptr;
            ItemEffectRequest request{};
            ItemEffectDrawData drawData;
            int vertexStart = 0;
            int indexStart = 0;
            unsigned frame = 0;
        };

        struct ItemEffectDrawCommandData
        {
            PendingItemEffect* effect = nullptr;
        };

        static_assert(std::is_trivially_copyable_v<ItemEffectDrawCommandData>);

        void RenderEffectDrawCallback(const ImDrawList*, const ImDrawCmd* command)
        {
            if (command == nullptr || command->UserCallbackData == nullptr ||
                command->UserCallbackDataSize != sizeof(EffectDrawCommand))
                return;

            EffectDrawCommand effectCommand{};
            std::memcpy(&effectCommand, command->UserCallbackData, sizeof(effectCommand));
            if (effectCommand.renderer != nullptr)
                effectCommand.renderer->RenderEffect(effectCommand.request);
        }

        void RenderItemEffectDrawCallback(const ImDrawList*, const ImDrawCmd* command)
        {
            if (command == nullptr || command->UserCallbackData == nullptr ||
                command->UserCallbackDataSize != sizeof(ItemEffectDrawCommandData))
                return;
            ItemEffectDrawCommandData callback{};
            std::memcpy(&callback, command->UserCallbackData, sizeof(callback));
            if (callback.effect && callback.effect->renderer && !callback.effect->drawData.Empty())
                callback.effect->renderer->RenderItemEffects(callback.effect->request,
                                                              callback.effect->drawData);
        }

        bool ReplacesSource(const ItemEffectRequest& request)
        {
            for (std::uint32_t index = 0; index < request.layerCount; ++index)
                if (request.layers[index].type == ItemEffectType::Blur &&
                    HasItemEffectFlags(request.layers[index].flags,
                                       ItemEffectFlags::ReplaceSource))
                    return true;
            return false;
        }

        void FinalizeItemEffect(PendingItemEffect& effect)
        {
            ImDrawList* drawList = effect.drawList;
            if (!drawList)
                return;
            const int vertexEnd = drawList->VtxBuffer.Size;
            const int indexEnd = drawList->IdxBuffer.Size;
            if (vertexEnd <= effect.vertexStart || indexEnd <= effect.indexStart)
                return;

            ItemEffectDrawData& output = effect.drawData;
            output.vertices.assign(drawList->VtxBuffer.Data + effect.vertexStart,
                                   drawList->VtxBuffer.Data + vertexEnd);
            ImVec2 boundsMin(FLT_MAX, FLT_MAX);
            ImVec2 boundsMax(-FLT_MAX, -FLT_MAX);

            for (const ImDrawCmd& command : drawList->CmdBuffer)
            {
                if (command.UserCallback != nullptr || command.ElemCount == 0)
                    continue;
                const int commandBegin = static_cast<int>(command.IdxOffset);
                const int commandEnd = commandBegin + static_cast<int>(command.ElemCount);
                const int begin = std::max(commandBegin, effect.indexStart);
                const int end = std::min(commandEnd, indexEnd);
                if (end <= begin)
                    continue;

                ItemEffectDrawCommand captured{};
                captured.textureId = command.GetTexID();
                captured.clipRect = command.ClipRect;
                captured.firstIndex = static_cast<std::uint32_t>(output.indices.size());

                ImVec2 commandMin(FLT_MAX, FLT_MAX);
                ImVec2 commandMax(-FLT_MAX, -FLT_MAX);
                for (int index = begin; index < end; ++index)
                {
                    const std::uint32_t absoluteVertex =
                        static_cast<std::uint32_t>(drawList->IdxBuffer[index]) + command.VtxOffset;
                    if (absoluteVertex < static_cast<std::uint32_t>(effect.vertexStart) ||
                        absoluteVertex >= static_cast<std::uint32_t>(vertexEnd))
                        continue;
                    const std::uint32_t localVertex = absoluteVertex - effect.vertexStart;
                    output.indices.push_back(localVertex);
                    const ImVec2 position = output.vertices[localVertex].pos;
                    commandMin.x = std::min(commandMin.x, position.x);
                    commandMin.y = std::min(commandMin.y, position.y);
                    commandMax.x = std::max(commandMax.x, position.x);
                    commandMax.y = std::max(commandMax.y, position.y);
                }
                captured.indexCount = static_cast<std::uint32_t>(output.indices.size()) -
                                      captured.firstIndex;
                if (captured.indexCount == 0)
                    continue;

                if (commandMax.x > commandMin.x && commandMax.y > commandMin.y)
                {
                    boundsMin.x = std::min(boundsMin.x, commandMin.x);
                    boundsMin.y = std::min(boundsMin.y, commandMin.y);
                    boundsMax.x = std::max(boundsMax.x, commandMax.x);
                    boundsMax.y = std::max(boundsMax.y, commandMax.y);
                }
                output.commands.push_back(captured);
            }

            if (boundsMax.x > boundsMin.x && boundsMax.y > boundsMin.y)
            {
                output.boundsMin = boundsMin;
                output.boundsMax = boundsMax;
            }
            if (ReplacesSource(effect.request) && effect.renderer &&
                HasEffectCapabilities(effect.renderer->GetCapabilities(),
                                      EffectCapabilities::ItemSourceReplacement))
                for (int index = effect.vertexStart; index < vertexEnd; ++index)
                    drawList->VtxBuffer[index].col &= ~(0xffu << IM_COL32_A_SHIFT);
        }
    }

    void SubmitEffect(ImDrawList* drawList, IEffectRenderer& renderer,
                      const EffectRequest& request)
    {
        if (drawList == nullptr)
            throw std::invalid_argument("IVUI::SubmitEffect requires a draw list.");

        const EffectDrawCommand command{&renderer, request};
        drawList->AddCallback(RenderEffectDrawCallback,
                              const_cast<EffectDrawCommand*>(&command), sizeof(command));
        drawList->AddCallback(ImDrawCallback_ResetRenderState);
    }

    void SubmitEffect(IEffectRenderer& renderer, const EffectRequest& request)
    {
        SubmitEffect(ImGui::GetWindowDrawList(), renderer, request);
    }

    ItemEffectScope::ItemEffectScope(ItemEffectScope&& other) noexcept
        : drawList_(other.drawList_), data_(std::move(other.data_)),
          contextLifetime_(std::move(other.contextLifetime_)),
          context_(other.context_), active_(other.active_)
    {
        other.drawList_ = nullptr;
        other.context_ = nullptr;
        other.active_ = false;
    }

    ItemEffectScope& ItemEffectScope::operator=(ItemEffectScope&& other) noexcept
    {
        if (this != &other)
        {
            End();
            drawList_ = other.drawList_;
            data_ = std::move(other.data_);
            contextLifetime_ = std::move(other.contextLifetime_);
            context_ = other.context_;
            active_ = other.active_;
            other.drawList_ = nullptr;
            other.context_ = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    ItemEffectScope::~ItemEffectScope()
    {
        End();
    }

    void ItemEffectScope::End() noexcept
    {
        if (!active_)
            return;
        auto* effect = static_cast<PendingItemEffect*>(data_.get());
        Context* context = contextLifetime_.expired()
            ? nullptr : static_cast<Context*>(context_);
        if (effect && context && GetContext() == context && context->frameActive &&
            effect->drawList == drawList_ && effect->frame == context->frameCount)
        {
            try
            {
                FinalizeItemEffect(*effect);
            }
            catch (...)
            {
                effect->drawData = {};
            }
        }
        if (context && context->activeItemEffect == effect)
            context->activeItemEffect = nullptr;
        drawList_ = nullptr;
        data_.reset();
        contextLifetime_.reset();
        context_ = nullptr;
        active_ = false;
    }

    ItemEffectScope BeginItemEffect(ImDrawList* drawList, IEffectRenderer& renderer,
                                    const ItemEffectRequest& request)
    {
        ItemEffectScope scope;
        Context* context = GetContext();
        if (!drawList || !context || context->activeItemEffect || request.layerCount == 0 ||
            request.layerCount > request.layers.size() ||
            !HasEffectCapabilities(renderer.GetCapabilities(), EffectCapabilities::ItemEffects))
            return scope;

        auto effect = std::make_shared<PendingItemEffect>();
        effect->renderer = &renderer;
        effect->drawList = drawList;
        effect->request = request;
        effect->vertexStart = drawList->VtxBuffer.Size;
        effect->indexStart = drawList->IdxBuffer.Size;
        effect->frame = context->frameCount;
        PendingItemEffect* effectPointer = effect.get();
        context->itemEffectData.push_back(effect);
        context->activeItemEffect = effectPointer;

        const ItemEffectDrawCommandData callback{effectPointer};
        drawList->AddCallback(RenderItemEffectDrawCallback,
                              const_cast<ItemEffectDrawCommandData*>(&callback), sizeof(callback));
        drawList->AddCallback(ImDrawCallback_ResetRenderState);

        scope.drawList_ = drawList;
        scope.data_ = std::move(effect);
        scope.contextLifetime_ = context->lifetime;
        scope.context_ = context;
        scope.active_ = true;
        return scope;
    }

    ItemEffectScope BeginItemEffect(IEffectRenderer& renderer,
                                    const ItemEffectRequest& request)
    {
        return BeginItemEffect(ImGui::GetWindowDrawList(), renderer, request);
    }
}
