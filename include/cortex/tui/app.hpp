#pragma once
#include "cortex/core/types.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <queue>
#include <mutex>
#include <string>

namespace cortex {

class Orchestrator;

class App {
public:
    explicit App(Orchestrator& orchestrator);
    void run();

    void post_event(UiEvent event);

private:
    ftxui::Component build_ui();
    void process_events();
    void send_message(const std::string& text);

    Orchestrator& orchestrator_;
    ftxui::ScreenInteractive screen_;

    // Chat state
    struct ChatMessage {
        std::string role;  // "user", "plan", "cortex", "error"
        std::string content;
    };
    std::vector<ChatMessage> messages_;
    std::string input_text_;
    bool show_timeline_ = true;

    // Status state
    std::string current_phase_ = "idle";
    int action_count_ = 0;
    int total_tokens_ = 0;

    // Thread-safe event queue
    std::queue<UiEvent> event_queue_;
    std::mutex event_mutex_;
};

} // namespace cortex
