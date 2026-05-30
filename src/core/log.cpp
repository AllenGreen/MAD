#include "core/log.hpp"

#include <chrono>
#include <cstdio>

namespace mad::core::log {

static Level s_level = Level::Debug;

void set_level(Level level) {
    s_level = level;
}

Level get_level() {
    return s_level;
}

void output(Level level, std::string_view tag, std::string_view message) {
    if (level < s_level) return;

    const char* level_str = "???";
    switch (level) {
        case Level::Debug: level_str = "DBG"; break;
        case Level::Info:  level_str = "INF"; break;
        case Level::Warn:  level_str = "WRN"; break;
        case Level::Error: level_str = "ERR"; break;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm tm_buf;
    localtime_r(&time, &tm_buf);

    std::fprintf(stderr, "%02d:%02d:%02d.%03d [%s] %.*s: %.*s\n",
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()),
        level_str,
        static_cast<int>(tag.size()), tag.data(),
        static_cast<int>(message.size()), message.data());
}

} // namespace mad::core::log
