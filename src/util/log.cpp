#include "cortex/util/log.hpp"
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>

namespace cortex::log {

static Level current_level = Level::Info;
static std::string log_file_path;
static std::mutex log_mutex;

void set_level(Level level) { current_level = level; }
void set_file(const std::string& path) { log_file_path = path; }

static void write(const std::string& level_str, const std::string& msg) {
    std::lock_guard lock(log_mutex);
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));

    std::string line = std::string("[") + buf + "] [" + level_str + "] " + msg + "\n";

    if (!log_file_path.empty()) {
        std::ofstream f(log_file_path, std::ios::app);
        f << line;
    }
    // Also write to stderr for debug builds
#ifndef NDEBUG
    std::cerr << line;
#endif
}

void debug(const std::string& msg) { if (current_level <= Level::Debug) write("DBG", msg); }
void info(const std::string& msg)  { if (current_level <= Level::Info)  write("INF", msg); }
void warn(const std::string& msg)  { if (current_level <= Level::Warn)  write("WRN", msg); }
void error(const std::string& msg) { if (current_level <= Level::Error) write("ERR", msg); }

} // namespace cortex::log
