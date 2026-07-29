#include <ImGuiVectorUI/ImGuiVectorUI.h>

#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<IVUI::UI>);
    static_assert(!std::is_move_constructible_v<IVUI::UI>);
    return 0;
}
