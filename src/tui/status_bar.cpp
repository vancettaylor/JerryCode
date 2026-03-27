#include "cortex/tui/status_bar.hpp"
#include "cortex/tui/theme.hpp"
#include <ftxui/dom/elements.hpp>

namespace cortex {

using namespace ftxui;

ftxui::Element render_status_bar(
    const std::string& phase,
    int action_count,
    int total_tokens,
    const std::string& provider_name) {
    return hbox({
        text(" CORTEX ") | bold | color(theme::accent()) | inverted,
        text(" | "),
        text(phase),
        text(" | Actions: " + std::to_string(action_count)),
        text(" | Tokens: " + std::to_string(total_tokens)),
        filler(),
        text(provider_name + " "),
    }) | bgcolor(Color::GrayDark);
}

} // namespace cortex
