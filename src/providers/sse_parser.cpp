#include "cortex/providers/sse_parser.hpp"

namespace cortex {

SseParser::SseParser(EventCallback on_event) : on_event_(std::move(on_event)) {}

void SseParser::feed(const std::string& chunk) {
    buffer_ += chunk;
    std::string::size_type pos;
    while ((pos = buffer_.find('\n')) != std::string::npos) {
        auto line = buffer_.substr(0, pos);
        buffer_ = buffer_.substr(pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        process_line(line);
    }
}

void SseParser::process_line(const std::string& line) {
    if (line.empty()) {
        dispatch_event();
        return;
    }

    if (line[0] == ':') return; // Comment

    auto colon = line.find(':');
    std::string field, value;
    if (colon != std::string::npos) {
        field = line.substr(0, colon);
        value = line.substr(colon + 1);
        if (!value.empty() && value[0] == ' ') value = value.substr(1);
    } else {
        field = line;
    }

    if (field == "event") current_event_ = value;
    else if (field == "data") {
        if (!current_data_.empty()) current_data_ += "\n";
        current_data_ += value;
    }
    else if (field == "id") current_id_ = value;
}

void SseParser::dispatch_event() {
    if (current_data_.empty() && current_event_.empty()) return;

    if (on_event_) {
        on_event_(SseEvent{
            .event = current_event_.empty() ? "message" : current_event_,
            .data = current_data_,
            .id = current_id_
        });
    }

    current_event_.clear();
    current_data_.clear();
}

void SseParser::reset() {
    buffer_.clear();
    current_event_.clear();
    current_data_.clear();
    current_id_.clear();
}

} // namespace cortex
