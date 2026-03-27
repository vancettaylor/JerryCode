#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace cortex {

struct ChatMessageData {
    std::string role;
    std::string content;
};

ftxui::Element render_chat_panel(const std::vector<ChatMessageData>& messages);

} // namespace cortex
