#include "cortex/tui/input_area.hpp"
#include <ftxui/component/component.hpp>

namespace cortex {

ftxui::Component make_input_area(std::string* content,
                                  std::function<void()> on_submit) {
    auto input = ftxui::Input(content, "Type a message...");
    return ftxui::CatchEvent(input, [on_submit](ftxui::Event event) -> bool {
        if (event == ftxui::Event::Return && on_submit) {
            on_submit();
            return true;
        }
        return false;
    });
}

} // namespace cortex
