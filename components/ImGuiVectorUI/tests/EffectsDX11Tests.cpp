#include "ImGuiVectorUI/Backends/EffectsDX11.h"

#include <d3d11.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
    std::string error;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context);
    if (FAILED(result))
    {
        result = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            &device, &featureLevel, &context);
    }
    if (FAILED(result))
    {
        std::cerr << "D3D11CreateDevice failed.\n";
        return EXIT_FAILURE;
    }

    auto embeddedRenderer = IVUI::DX11::EffectRenderer::Create({device}, &error);
    if (!embeddedRenderer)
    {
        std::cerr << error << '\n';
        context->Release();
        device->Release();
        return EXIT_FAILURE;
    }

    context->Release();
    device->Release();
    return EXIT_SUCCESS;
}
