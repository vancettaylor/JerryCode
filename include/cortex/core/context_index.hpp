#pragma once
#include "cortex/core/types.hpp"
#include <string>
#include <vector>

namespace cortex {

class ContextIndex {
public:
    void set_project_root(const std::string& path);
    void set_active_task(const std::string& task_description);
    void update_file_tree(const std::vector<std::string>& paths);
    void add_action_summary(const TimelineEntry& entry);
    void add_pointer(const ContextPointer& pointer);
    void remove_pointer(const std::string& label);
    void mark_expanded(const std::string& label, bool expanded);

    [[nodiscard]] std::string render() const;
    [[nodiscard]] std::string render_pointers() const;
    [[nodiscard]] int estimated_tokens() const;
    [[nodiscard]] const std::vector<TimelineEntry>& recent_actions() const;
    [[nodiscard]] const std::vector<ContextPointer>& pointers() const;
    [[nodiscard]] const std::string& project_root() const { return project_root_; }
    [[nodiscard]] const std::string& active_task() const { return active_task_; }

private:
    std::string project_root_;
    std::string active_task_;
    std::vector<std::string> file_tree_;
    std::vector<TimelineEntry> recent_;
    std::vector<ContextPointer> pointers_;
    int max_recent_ = 10;
    int max_pointers_ = 20;
};

} // namespace cortex
