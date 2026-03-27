#pragma once
#include "cortex/core/types.hpp"
#include <ftxui/dom/elements.hpp>
#include <vector>

namespace cortex {

ftxui::Element render_timeline_panel(const std::vector<TimelineEntry>& entries);

} // namespace cortex
