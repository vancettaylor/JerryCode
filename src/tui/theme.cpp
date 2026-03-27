#include "cortex/tui/theme.hpp"

namespace cortex::theme {

ftxui::Color accent()          { return ftxui::Color::Cyan; }
ftxui::Color error()           { return ftxui::Color::Red; }
ftxui::Color success()         { return ftxui::Color::Green; }
ftxui::Color muted()           { return ftxui::Color::GrayDark; }
ftxui::Color phase_planning()  { return ftxui::Color::Yellow; }
ftxui::Color phase_executing() { return ftxui::Color::Cyan; }
ftxui::Color phase_reviewing() { return ftxui::Color::Magenta; }

} // namespace cortex::theme
