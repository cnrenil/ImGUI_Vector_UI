/** @file Section.h @brief Collapsible labeled group widget. */
#pragma once

#include <functional>

namespace IVUI
{
    /** @brief Draws a collapsible section and toggles @p open on header click.
     *  @return True on the toggle frame.
     */
    bool Section(const char* label, bool* open, const std::function<void()>& body);
}
