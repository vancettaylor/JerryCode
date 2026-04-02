#pragma once
/**
 * @file app.hpp
 * @brief Main TUI application — OpenCode-style terminal interface for JerryCode.
 *
 * Layout:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │ JERRYCODE │ model │ phase │ tasks 3/5 │ tokens: 1.2k │ 42s │  StatusBar
 *   ├───────────────────────────────────┬─────────────────────────┤
 *   │ [user] Fix the bug in parser.cpp  │  Task List              │
 *   │                                   │  [x] Read parser.cpp    │
 *   │ [step] Reading parser.cpp...      │  [>] Fix indexing bug   │  TaskPanel
 *   │ [code] #include <iostream>        │  [ ] Compile            │
 *   │        void parse() { ...         │  [ ] Run tests          │
 *   │                                   │                         │
 *   │ [step] Writing fix...             │  Context                │
 *   │ [ok] Fixed parser.cpp (234 bytes) │  [+] parser.cpp  120tok │  ContextPanel
 *   │                                   │  [-] main.cpp           │
 *   │ [error] Compile failed...         │  [-] utils.hpp          │
 *   ├───────────────────────────────────┴─────────────────────────┤
 *   │ > Type a message... (Enter to send, Ctrl+C to quit)        │  InputArea
 *   └─────────────────────────────────────────────────────────────┘
 *
 * The TUI runs on the main thread. Session runs on a background thread.
 * Communication is via a thread-safe event queue using FTXUI's PostEvent.
 */

#include "cortex/core/session.hpp"
#include "cortex/util/config.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <queue>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <thread>

namespace cortex {

/// A single message in the chat panel.
struct ChatMessage {
    std::string role;     ///< "user", "step", "code", "ok", "error", "status", "stats"
    std::string content;  ///< The message text
};

/// Events posted from the session thread to the TUI thread.
struct TuiEvent {
    enum Type { Phase, Stream, Status, Error, TaskList, Stats, Done };
    Type type;
    std::string text;
};

/**
 * @class TuiApp
 * @brief The main terminal user interface, modeled after OpenCode.
 *
 * Usage:
 *   Session session(...);
 *   TuiApp app(session, config);
 *   app.run();  // Blocks until user quits
 */
class TuiApp {
public:
    TuiApp(const std::string& project_root,
           const ProviderConfig& provider_config,
           AgentRegistry& agents,
           const AppConfig& config);
    ~TuiApp();

    /// Run the TUI event loop (blocks until quit).
    void run();

private:
    /// Build the FTXUI component tree.
    ftxui::Component build_ui();

    /// Process queued events from the session thread.
    void process_events();

    /// Send user input to the session.
    void submit_input();

    /// Post an event from the session thread to the TUI.
    void post(TuiEvent::Type type, const std::string& text);

    // ─── UI state ──────────────────────────────────────────
    ftxui::ScreenInteractive screen_;
    ftxui::Component input_component_;      ///< Input field component (must outlive renderer)
    std::vector<ChatMessage> messages_;     ///< Chat history
    std::string input_text_;                ///< Current input buffer
    std::string current_phase_ = "idle";    ///< Current session phase
    std::string task_list_text_;            ///< Rendered task list
    std::string stats_text_;                ///< Session statistics
    std::string model_name_;                ///< Active model name
    int scroll_offset_ = 0;                 ///< Chat scroll position
    bool show_sidebar_ = true;              ///< Sidebar visibility
    std::atomic<bool> session_running_{false}; ///< Is a session active?

    // ─── Event queue ───────────────────────────────────────
    std::queue<TuiEvent> event_queue_;
    std::mutex event_mutex_;

    // ─── Session management ────────────────────────────────
    std::string project_root_;
    ProviderConfig provider_config_;
    AgentRegistry& agents_;
    AppConfig config_;
    std::unique_ptr<std::thread> session_thread_;
};

} // namespace cortex
