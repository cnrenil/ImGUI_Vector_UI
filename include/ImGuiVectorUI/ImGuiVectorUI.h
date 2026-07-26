// ImGuiVectorUI — single public entry point.
//
// An animation-driven, ImDrawList-rendered widget library for Dear ImGui.
// Every control is hand-drawn against a semantic theme, with per-widget easing
// keyed by ImGui ID. Immediate-mode, namespace IVUI.
//
// Host usage:
//   IVUI::CreateContext();                 // after ImGui_ImplXXX_Init
//   ... per frame, after ImGui::NewFrame(): IVUI::NewFrame();
//   IVUI::DestroyContext();                // before ImGui::DestroyContext()
#pragma once

#include "ImGuiVectorUI/Core/Types.h"
#include "ImGuiVectorUI/Core/Easing.h"
#include "ImGuiVectorUI/Core/AnimState.h"
#include "ImGuiVectorUI/Core/Context.h"
#include "ImGuiVectorUI/Core/Id.h"
#include "ImGuiVectorUI/Core/Motion.h"
#include "ImGuiVectorUI/Core/DragDrop.h"

#include "ImGuiVectorUI/Theme/Theme.h"

#include "ImGuiVectorUI/Drawing/DrawText.h"
#include "ImGuiVectorUI/Drawing/Primitives.h"
#include "ImGuiVectorUI/Drawing/Icons.h"
#include "ImGuiVectorUI/Drawing/Tooltip.h"
#include "ImGuiVectorUI/Drawing/Effects.h"

#include "ImGuiVectorUI/Layout/AutoSizeChild.h"
#include "ImGuiVectorUI/Layout/ScrollRegion.h"
#include "ImGuiVectorUI/Layout/Panel.h"
#include "ImGuiVectorUI/Layout/Section.h"
#include "ImGuiVectorUI/Layout/Tabs.h"
#include "ImGuiVectorUI/Layout/Table.h"
#include "ImGuiVectorUI/Layout/StatusChips.h"

#include "ImGuiVectorUI/Widgets/Toggle.h"
#include "ImGuiVectorUI/Widgets/Slider.h"
#include "ImGuiVectorUI/Widgets/Combo.h"
#include "ImGuiVectorUI/Widgets/Button.h"
#include "ImGuiVectorUI/Widgets/ColorPicker.h"
#include "ImGuiVectorUI/Widgets/HotkeyInput.h"
#include "ImGuiVectorUI/Widgets/TextInput.h"
#include "ImGuiVectorUI/Widgets/Keybind.h"
#include "ImGuiVectorUI/Widgets/Popup.h"
#include "ImGuiVectorUI/Widgets/Toast.h"
