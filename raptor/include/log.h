#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace raptor::log {

    enum class Level { Trace, Debug, Info, Warn, Error, Fatal, Off };

    enum class ColorMode { Auto, Always, Never };

    // Messages below this level are dropped. Default: Info.
    void set_level(Level level);
    Level level();

    // Auto: color only when the stream is a terminal and NO_COLOR is unset.
    void set_color_mode(ColorMode mode);

    // Thread-safe. Warn and above go to stderr, the rest to stdout.
    void write(Level level, std::string_view message);

    inline bool enabled(Level lvl) { return lvl >= level() && lvl != Level::Off; }

    template <class... Args>
    void print(Level lvl, std::format_string<Args...> fmt, Args&&... args) {
        if (!enabled(lvl)) return;
        write(lvl, std::format(fmt, std::forward<Args>(args)...));
    }

    template <class... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args) { print(Level::Trace, fmt, std::forward<Args>(args)...); }
    template <class... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) { print(Level::Debug, fmt, std::forward<Args>(args)...); }
    template <class... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) { print(Level::Info, fmt, std::forward<Args>(args)...); }
    template <class... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) { print(Level::Warn, fmt, std::forward<Args>(args)...); }
    template <class... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) { print(Level::Error, fmt, std::forward<Args>(args)...); }
    template <class... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) { print(Level::Fatal, fmt, std::forward<Args>(args)...); }
}
