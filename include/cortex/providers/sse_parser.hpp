/**
 * @file sse_parser.hpp
 * @brief Incremental Server-Sent Events (SSE) stream parser.
 */

#pragma once
#include <string>
#include <functional>
#include <optional>

namespace cortex {

/** @brief A single parsed SSE event. */
struct SseEvent {
    std::string event;  ///< Event type (e.g. "message", "error").
    std::string data;   ///< Event payload data.
    std::string id;     ///< Optional event ID.
};

/**
 * @brief Incremental parser for the Server-Sent Events protocol.
 *
 * Feed raw HTTP response chunks via feed() and receive parsed SseEvent
 * objects through the registered callback.
 */
class SseParser {
public:
    /// Callback type invoked for each fully parsed SSE event.
    using EventCallback = std::function<void(const SseEvent&)>;

    /**
     * @brief Construct a parser with the given event callback.
     * @param on_event Callback invoked for each complete SSE event.
     */
    explicit SseParser(EventCallback on_event);

    /**
     * @brief Feed a raw data chunk from the HTTP response stream.
     * @param chunk Raw bytes to parse; may contain partial events.
     */
    void feed(const std::string& chunk);

    /** @brief Reset parser state, discarding any buffered partial event. */
    void reset();

private:
    void process_line(const std::string& line);
    void dispatch_event();

    EventCallback on_event_;    ///< User-provided event callback.
    std::string current_event_; ///< Accumulator for the current event type.
    std::string current_data_;  ///< Accumulator for the current event data.
    std::string current_id_;    ///< Accumulator for the current event ID.
    std::string buffer_;        ///< Buffer for incomplete lines across chunks.
};

} // namespace cortex
