/**
 * @file app.cpp
 * @brief TUI implementation — OpenCode-style terminal interface for JerryCode.
 */

#include "cortex/tui/app.hpp"
#include "cortex/tui/theme.hpp"
#include "cortex/providers/openai.hpp"
#include "cortex/providers/anthropic.hpp"
#include "cortex/util/log.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <sstream>
#include <algorithm>

namespace cortex {

using namespace ftxui;

// ─── Construction / Destruction ───────────────────────────────

TuiApp::TuiApp(const std::string& project_root,
               const ProviderConfig& provider_config,
               AgentRegistry& agents,
               const AppConfig& config)
    : screen_(ScreenInteractive::Fullscreen())
    , project_root_(project_root)
    , provider_config_(provider_config)
    , agents_(agents)
    , config_(config)
    , model_name_(config.model)
{
    messages_.push_back({"status",
        "JerryCode — context-managed coding agent\n"
        "Model: " + config.model + " | Project: " + project_root + "\n"
        "Type a task and press Enter. /sidebar to toggle. /clear to reset. Ctrl+C to quit."});
}

TuiApp::~TuiApp() {
    if (session_thread_ && session_thread_->joinable()) {
        session_thread_->join();
    }
}

// ─── Event Queue ──────────────────────────────────────────────

void TuiApp::post(TuiEvent::Type type, const std::string& text) {
    {
        std::lock_guard lock(event_mutex_);
        event_queue_.push({type, text});
    }
    screen_.PostEvent(Event::Custom);
}

void TuiApp::process_events() {
    std::lock_guard lock(event_mutex_);
    while (!event_queue_.empty()) {
        auto& ev = event_queue_.front();
        switch (ev.type) {
        case TuiEvent::Phase:
            current_phase_ = ev.text;
            messages_.push_back({"step", ev.text});
            break;
        case TuiEvent::Stream:
            if (!messages_.empty() && messages_.back().role == "code") {
                messages_.back().content += ev.text;
            } else {
                messages_.push_back({"code", ev.text});
            }
            break;
        case TuiEvent::Status:
            if (ev.text.find("[") != std::string::npos && ev.text.find("]") != std::string::npos) {
                task_list_text_ = ev.text;
            } else {
                messages_.push_back({"status", ev.text});
            }
            break;
        case TuiEvent::Error:
            messages_.push_back({"error", ev.text});
            break;
        case TuiEvent::TaskList:
            task_list_text_ = ev.text;
            break;
        case TuiEvent::Stats:
            stats_text_ = ev.text;
            break;
        case TuiEvent::Done:
            current_phase_ = "idle";
            session_running_ = false;
            if (!ev.text.empty()) {
                messages_.push_back({"stats", ev.text});
            }
            break;
        }
        event_queue_.pop();
    }
}

// ─── Session Submission ───────────────────────────────────────

void TuiApp::submit_input() {
    auto text = input_text_;
    input_text_.clear();

    if (text.empty()) return;
    if (session_running_) {
        messages_.push_back({"error", "Session already running. Wait for it to finish."});
        return;
    }

    // Handle special commands
    if (text == "/quit" || text == "/exit") {
        screen_.Exit();
        return;
    }
    if (text == "/sidebar") {
        show_sidebar_ = !show_sidebar_;
        return;
    }
    if (text == "/clear") {
        messages_.clear();
        task_list_text_.clear();
        stats_text_.clear();
        return;
    }

    messages_.push_back({"user", text});
    session_running_ = true;
    current_phase_ = "starting";

    // Run session on background thread
    auto request = text;
    if (session_thread_ && session_thread_->joinable()) {
        session_thread_->join();
    }
    session_thread_ = std::make_unique<std::thread>([this, request]() {
        std::unique_ptr<IProvider> provider;
        if (config_.provider == "anthropic") {
            provider = std::make_unique<AnthropicProvider>();
        } else {
            provider = std::make_unique<OpenAiProvider>();
        }

        Session session(std::move(provider), agents_, project_root_, provider_config_);

        SessionCallbacks cb{
            .on_phase = [this](const std::string& phase) {
                post(TuiEvent::Phase, phase);
            },
            .on_stream = [this](const std::string& text) {
                if (text.size() > 2000) {
                    post(TuiEvent::Stream, text.substr(0, 2000) + "\n...(truncated)");
                } else {
                    post(TuiEvent::Stream, text);
                }
            },
            .on_status = [this](const std::string& status) {
                post(TuiEvent::Status, status);
            },
            .on_error = [this](const std::string& err) {
                post(TuiEvent::Error, err);
            }
        };

        session.run(request, cb);
        post(TuiEvent::Stats, session.stats_summary());
        post(TuiEvent::Done, "");
    });
}

// ─── UI Building ──────────────────────────────────────────────

ftxui::Component TuiApp::build_ui() {
    // Create input as a member so it outlives this function
    auto input_option = InputOption::Default();
    input_component_ = Input(&input_text_, "Type a task...", input_option);

    // Wrap with Enter key handling
    input_component_ = CatchEvent(input_component_, [this](Event event) -> bool {
        if (event == Event::Return) {
            submit_input();
            return true;
        }
        return false;
    });

    // Use 'this' pointer to access input_component_ safely (no dangling ref)
    auto renderer = Renderer(input_component_, [this] {
        process_events();

        // ─── Status Bar ────────────────────────────────
        auto status_bar = hbox({
            text(" JERRYCODE ") | bold | color(Color::Black) | bgcolor(Color::Cyan),
            text(" "),
            text(model_name_) | color(Color::Yellow),
            text(" | "),
            text(current_phase_) | color(
                current_phase_ == "idle" ? Color::GrayDark :
                current_phase_ == "done" ? Color::Green : Color::Cyan),
            filler(),
            text(session_running_ ? " RUNNING " : " IDLE ") |
                color(session_running_ ? Color::Green : Color::GrayDark),
        }) | bgcolor(Color::GrayDark);

        // ─── Chat Panel ────────────────────────────────
        Elements chat_elems;
        if (messages_.empty()) {
            chat_elems.push_back(text("") | flex);
        }
        for (const auto& msg : messages_) {
            Color c = Color::White;
            std::string prefix;
            if (msg.role == "user")        { c = Color::Cyan;      prefix = " > "; }
            else if (msg.role == "step")   { c = Color::Yellow;    prefix = " * "; }
            else if (msg.role == "code")   { c = Color::Green;     prefix = "   "; }
            else if (msg.role == "ok")     { c = Color::Green;     prefix = " + "; }
            else if (msg.role == "error")  { c = Color::Red;       prefix = " ! "; }
            else if (msg.role == "status") { c = Color::GrayLight; prefix = " - "; }
            else if (msg.role == "stats")  { c = Color::Magenta;   prefix = "   "; }

            auto content = msg.content;
            if (content.size() > 2000) content = content.substr(0, 2000) + "\n...(truncated)";
            chat_elems.push_back(paragraph(prefix + content) | color(c));
        }

        auto chat_panel = vbox(chat_elems) | yframe | flex;

        // ─── Sidebar ──────────────────────────────────
        Elements sidebar_elems;
        sidebar_elems.push_back(text("JerryCode") | bold | color(Color::Cyan));
        sidebar_elems.push_back(separator());

        if (!task_list_text_.empty()) {
            sidebar_elems.push_back(text("Tasks") | bold | color(Color::Yellow));
            std::istringstream ss(task_list_text_);
            std::string line;
            while (std::getline(ss, line)) {
                Color tc = Color::White;
                if (line.find("[x]") != std::string::npos) tc = Color::Green;
                else if (line.find("[>]") != std::string::npos) tc = Color::Cyan;
                else if (line.find("[!]") != std::string::npos) tc = Color::Red;
                sidebar_elems.push_back(text(line) | color(tc));
            }
            sidebar_elems.push_back(text(""));
        } else {
            sidebar_elems.push_back(text("No tasks yet") | dim);
        }

        if (!stats_text_.empty()) {
            sidebar_elems.push_back(separator());
            sidebar_elems.push_back(text("Stats") | bold | color(Color::Magenta));
            std::istringstream ss(stats_text_);
            std::string line;
            while (std::getline(ss, line)) {
                sidebar_elems.push_back(text(line) | dim);
            }
        }

        sidebar_elems.push_back(filler());
        sidebar_elems.push_back(separator());
        sidebar_elems.push_back(text("/sidebar /clear /quit") | dim);

        auto sidebar = vbox(sidebar_elems) | size(WIDTH, EQUAL, 30) | border;

        // ─── Input Area ────────────────────────────────
        auto input_box = hbox({
            text(session_running_ ? " * " : " > ") |
                color(session_running_ ? Color::Yellow : Color::Cyan),
            input_component_->Render() | flex,
        }) | border;

        // ─── Compose ──────────────────────────────────
        Element main_area;
        if (show_sidebar_) {
            main_area = hbox({ chat_panel | flex, separator(), sidebar });
        } else {
            main_area = chat_panel | flex;
        }

        return vbox({
            status_bar,
            separator(),
            main_area | flex,
            separator(),
            input_box,
        });
    });

    return renderer;
}

// ─── Run ──────────────────────────────────────────────────────

void TuiApp::run() {
    auto ui = build_ui();
    screen_.Loop(ui);
}

} // namespace cortex
