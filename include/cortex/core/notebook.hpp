/**
 * @file notebook.hpp
 * @brief Session notebook for accumulating findings, observations, and research notes.
 *
 * The model uses @note(text) to record observations as it works.
 * Notes persist across all tasks in a session and survive context rotation.
 * They feed into final reports and maintain continuity over long tasks.
 */
#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace cortex {

/// A single note entry.
struct Note {
    int id;
    std::string text;                              ///< The note content.
    std::string source;                            ///< What task/file produced this note.
    std::chrono::steady_clock::time_point created;  ///< When the note was created.
};

/**
 * @brief Persistent notebook for a session.
 *
 * Provides append, search, and rendering for use in prompts.
 * Notes accumulate over the session and are never deleted (only summarized).
 */
class Notebook {
public:
    /// Add a note.
    /// @param text The note content.
    /// @param source Context about where/why this note was made.
    /// @return The assigned note ID.
    int add(const std::string& text, const std::string& source = "");

    /// Get all notes.
    const std::vector<Note>& all() const { return notes_; }

    /// Render notes as a string for prompt inclusion.
    std::string render() const;

    /// Render a compact summary (just first line of each note).
    std::string render_compact() const;

    /// Count of notes.
    int count() const { return static_cast<int>(notes_.size()); }

    /// Clear all notes.
    void clear() { notes_.clear(); next_id_ = 1; }

private:
    std::vector<Note> notes_;
    int next_id_ = 1;
};

} // namespace cortex
