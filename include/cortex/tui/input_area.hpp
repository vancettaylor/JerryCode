#pragma once
#include <ftxui/component/component.hpp>
#include <string>
#include <functional>

namespace cortex {

ftxui::Component make_input_area(
    std::string* content,
    std::function<void()> on_submit);

} // namespace cortex
