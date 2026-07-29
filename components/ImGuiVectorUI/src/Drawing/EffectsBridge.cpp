#include "ImGuiVectorUI/Drawing/Effects.h"

#include "imgui.h"

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
}
