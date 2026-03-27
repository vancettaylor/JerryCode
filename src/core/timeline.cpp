#include "cortex/core/timeline.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cortex {

void Timeline::append(const TimelineEntry& entry) {
    TimelineEntry e = entry;
    e.sequence_number = ++sequence_counter_;
    entries_.push_back(e);
}

const std::vector<TimelineEntry>& Timeline::entries() const { return entries_; }

std::vector<TimelineEntry> Timeline::last_n(int n) const {
    if (static_cast<int>(entries_.size()) <= n) return entries_;
    return std::vector<TimelineEntry>(entries_.end() - n, entries_.end());
}

std::string Timeline::render_compact() const {
    std::ostringstream ss;
    ss << "#   | Action                              | Tool       | OK | Tokens\n";
    ss << "----|-------------------------------------|------------|----|---------\n";
    for (const auto& e : entries_) {
        ss << std::left << std::setw(4) << e.sequence_number << "| "
           << std::setw(36) << e.one_line_summary.substr(0, 35) << "| "
           << std::setw(11) << e.tool_used << "| "
           << (e.success ? "Y " : "N ") << "| "
           << e.token_cost << "\n";
    }
    return ss.str();
}

int Timeline::total_tokens_spent() const {
    int total = 0;
    for (const auto& e : entries_) total += e.token_cost;
    return total;
}

int Timeline::action_count() const { return static_cast<int>(entries_.size()); }

TimelineReviewer::ReviewResult TimelineReviewer::review(
    const Timeline& timeline,
    const ContextIndex& index,
    IProvider& provider,
    const ContextCompiler& compiler) {
    // TODO: Full implementation with LLM call
    return ReviewResult{
        .course_correction_needed = false,
        .assessment = "Timeline review not yet implemented"
    };
}

} // namespace cortex
