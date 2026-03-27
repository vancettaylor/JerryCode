#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>

namespace cortex {

ftxui::Element render_status_bar(
    const std::string& phase,
    int action_count,
    int total_tokens,
    const std::string& provider_name = "anthropic");

} // namespace cortex
