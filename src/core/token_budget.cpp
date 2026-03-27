#include "cortex/core/token_budget.hpp"
#include <string>

namespace cortex {

int estimate_tokens(const std::string& text) {
    // Rough estimate: ~4 characters per token for English
    return static_cast<int>(text.size()) / 4;
}

} // namespace cortex
