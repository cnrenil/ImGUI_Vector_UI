#include <ImGuiVectorUI/ImGuiVectorUI.h>
#include <ImGuiVectorUI/Backends/PlatformWin32.h>
#include <ImGuiVectorUI/Backends/RendererDX11.h>
#include <ImGuiVectorUI/Backends/RendererDX12.h>

#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<IVUI::UI>);
    return 0;
}
