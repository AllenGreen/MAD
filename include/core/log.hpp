#pragma once

#include <cstdio>
#include <format>
#include <string_view>

namespace mad::core::log {

enum class Level { Debug, Info, Warn, Error };

void set_level(Level level);
Level get_level();

void output(Level level, std::string_view tag, std::string_view message);

template<typename... Args>
void debug(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Debug)
        output(Level::Debug, tag, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void info(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Info)
        output(Level::Info, tag, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void warn(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    if (get_level() <= Level::Warn)
        output(Level::Warn, tag, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void error(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    output(Level::Error, tag, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace mad::core::log
