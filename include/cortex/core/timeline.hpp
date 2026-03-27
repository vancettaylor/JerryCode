#pragma once
#include "cortex/core/types.hpp"
#include "cortex/providers/provider.hpp"
#include <vector>

namespace cortex {

class ContextIndex;
class ContextCompiler;

class Timeline {
public:
    void append(const TimelineEntry& entry);
    [[nodiscard]] const std::vector<TimelineEntry>& entries() const;
    [[nodiscard]] std::vector<TimelineEntry> last_n(int n) const;
    [[nodiscard]] std::string render_compact() const;
    [[nodiscard]] int total_tokens_spent() const;
    [[nodiscard]] int action_count() const;

private:
    std::vector<TimelineEntry> entries_;
    int sequence_counter_ = 0;
};

class TimelineReviewer {
public:
    struct ReviewResult {
        bool course_correction_needed = false;
        std::string assessment;
        std::vector<std::string> expand_requests;
        std::optional<std::string> new_task_description;
    };

    ReviewResult review(const Timeline& timeline,
                        const ContextIndex& index,
                        IProvider& provider,
                        const ContextCompiler& compiler);
};

} // namespace cortex
