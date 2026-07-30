#include "ImGuiVectorUI/Backends/EffectsDX12.h"

#include <d3d12.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
    std::string error;
    ID3D12Device* device = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&device))))
    {
        std::cerr << "D3D12CreateDevice failed.\n";
        return EXIT_FAILURE;
    }
    auto embeddedRenderer = IVUI::DX12::EffectRenderer::Create(
        {device, DXGI_FORMAT_R8G8B8A8_UNORM}, &error);
    if (!embeddedRenderer)
    {
        std::cerr << error << '\n';
        device->Release();
        return EXIT_FAILURE;
    }

    device->Release();
    return EXIT_SUCCESS;
}
