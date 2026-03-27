#pragma once
#include <string>
#include <functional>
#include <optional>

namespace cortex {

struct SseEvent {
    std::string event;
    std::string data;
    std::string id;
};

class SseParser {
public:
    using EventCallback = std::function<void(const SseEvent&)>;

    explicit SseParser(EventCallback on_event);

    void feed(const std::string& chunk);
    void reset();

private:
    void process_line(const std::string& line);
    void dispatch_event();

    EventCallback on_event_;
    std::string current_event_;
    std::string current_data_;
    std::string current_id_;
    std::string buffer_;
};

} // namespace cortex
