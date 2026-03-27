#include "cortex/tui/timeline_panel.hpp"
#include "cortex/tui/theme.hpp"
#include <ftxui/dom/elements.hpp>

namespace cortex {

using namespace ftxui;

ftxui::Element render_timeline_panel(const std::vector<TimelineEntry>& entries) {
    Elements elements;
    elements.push_back(text("Timeline") | bold);
    elements.push_back(separator());

    for (const auto& entry : entries) {
        auto line = hbox({
            text("#" + std::to_string(entry.sequence_number) + " "),
            text(entry.success ? "OK " : "FAIL ") |
                color(entry.success ? theme::success() : theme::error()),
            text(entry.one_line_summary.substr(0, 30)),
        });
        elements.push_back(line);
    }

    return vbox(elements) | border;
}

} // namespace cortex
