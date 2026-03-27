#pragma once
#include <string>

namespace cortex {

struct TokenBudget {
    int max_context_window = 200000;
    int reserved_for_output = 8192;
    int index_budget = 2000;
    int expansion_budget = 50000;
    int agent_budget = 30000;

    [[nodiscard]] int available_for_content() const {
        return max_context_window - reserved_for_output - index_budget;
    }
};

int estimate_tokens(const std::string& text);

} // namespace cortex
