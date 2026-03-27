#pragma once
#include <string>

namespace cortex::log {

enum class Level { Debug, Info, Warn, Error };

void set_level(Level level);
void set_file(const std::string& path);

void debug(const std::string& msg);
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);

} // namespace cortex::log
