#include "cortex/tui/app.hpp"
#include "cortex/tui/theme.hpp"
#include "cortex/core/orchestrator.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>

namespace cortex {

using namespace ftxui;

App::App(Orchestrator& orchestrator)
    : orchestrator_(orchestrator)
    , screen_(ScreenInteractive::Fullscreen()) {}

void App::post_event(UiEvent event) {
    {
        std::lock_guard lock(event_mutex_);
        event_queue_.push(std::move(event));
    }
    screen_.PostEvent(Event::Custom);
}

void App::process_events() {
    std::lock_guard lock(event_mutex_);
    while (!event_queue_.empty()) {
        auto& event = event_queue_.front();
        std::visit([this](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, StreamDelta>) {
                if (!messages_.empty() && messages_.back().role == "cortex") {
                    messages_.back().content += e.text;
                } else {
                    messages_.push_back({"cortex", e.text});
                }
            } else if constexpr (std::is_same_v<T, PlanReady>) {
                messages_.push_back({"plan", e.plan.task_description +
                    "\n  Tool: " + e.plan.tool_name +
                    "\n  Expected: " + e.plan.expected_outcome});
                action_count_++;
            } else if constexpr (std::is_same_v<T, ActionComplete>) {
                total_tokens_ += e.record.input_tokens + e.record.output_tokens;
            } else if constexpr (std::is_same_v<T, PhaseChange>) {
                current_phase_ = e.phase;
            } else if constexpr (std::is_same_v<T, ErrorEvent>) {
                messages_.push_back({"error", e.message});
            }
        }, event);
        event_queue_.pop();
    }
}

void App::send_message(const std::string& text) {
    if (text.empty()) return;
    messages_.push_back({"user", text});

    auto* app = this;
    std::thread([app, text]() {
        OrchestratorCallbacks callbacks{
            .on_phase_change = [app](const std::string& phase) {
                app->post_event(PhaseChange{phase});
            },
            .on_stream_text = [app](const std::string& delta) {
                app->post_event(StreamDelta{delta});
            },
            .on_plan_ready = [app](const ActionPlan& plan) {
                app->post_event(PlanReady{plan});
            },
            .on_action_complete = [app](const MetadataRecord& record) {
                app->post_event(ActionComplete{record});
            },
            .on_error = [app](const std::string& msg) {
                app->post_event(ErrorEvent{msg});
            }
        };
        app->orchestrator_.handle_user_input(text, callbacks);
    }).detach();
}

ftxui::Component App::build_ui() {
    auto input = Input(&input_text_, "Type a message...");

    auto component = CatchEvent(input, [&](Event event) -> bool {
        // Ctrl+Enter or just Enter to send
        if (event == Event::Return) {
            send_message(input_text_);
            input_text_.clear();
            return true;
        }
        return false;
    });

    auto renderer = Renderer(component, [&] {
        process_events();

        // Status bar
        auto status = hbox({
            text(" CORTEX ") | bold | color(theme::accent()) | inverted,
            text(" | Phase: " + current_phase_ + " "),
            text(" | Actions: " + std::to_string(action_count_) + " "),
            text(" | Tokens: " + std::to_string(total_tokens_) + " "),
            filler(),
        }) | bgcolor(Color::GrayDark);

        // Chat messages
        Elements chat_elements;
        for (const auto& msg : messages_) {
            Color c = Color::White;
            std::string prefix = "";
            if (msg.role == "user") { c = Color::Cyan; prefix = "[you] "; }
            else if (msg.role == "plan") { c = Color::Yellow; prefix = "[plan] "; }
            else if (msg.role == "cortex") { c = Color::Green; prefix = "[cortex] "; }
            else if (msg.role == "error") { c = Color::Red; prefix = "[error] "; }

            chat_elements.push_back(
                paragraph(prefix + msg.content) | color(c)
            );
        }

        auto chat = vbox(chat_elements) | yframe | flex;

        // Input area
        auto input_box = hbox({
            text("> "),
            component->Render() | flex,
        }) | border;

        return vbox({
            status,
            separator(),
            chat,
            separator(),
            input_box,
        });
    });

    return renderer;
}

void App::run() {
    auto ui = build_ui();
    screen_.Loop(ui);
}

} // namespace cortex
