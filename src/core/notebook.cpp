/**
 * @file notebook.cpp
 * @brief Session notebook implementation.
 */
#include "cortex/core/notebook.hpp"
#include <sstream>

namespace cortex {

int Notebook::add(const std::string& text, const std::string& source) {
    Note n;
    n.id = next_id_++;
    n.text = text;
    n.source = source;
    n.created = std::chrono::steady_clock::now();
    notes_.push_back(n);
    return n.id;
}

std::string Notebook::render() const {
    if (notes_.empty()) return "(no notes yet)";

    std::ostringstream ss;
    for (const auto& n : notes_) {
        ss << "[" << n.id << "]";
        if (!n.source.empty()) ss << " (" << n.source << ")";
        ss << " " << n.text << "\n";
    }
    return ss.str();
}

std::string Notebook::render_compact() const {
    if (notes_.empty()) return "(no notes)";

    std::ostringstream ss;
    for (const auto& n : notes_) {
        auto first_line = n.text.substr(0, n.text.find('\n'));
        if (first_line.size() > 80) first_line = first_line.substr(0, 77) + "...";
        ss << "[" << n.id << "] " << first_line << "\n";
    }
    return ss.str();
}

} // namespace cortex
