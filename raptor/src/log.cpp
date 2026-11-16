#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS // getenv
#endif
#endif

#include <log.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

namespace raptor::log {

namespace {

// How a stream renders color.
enum class Backend { None, Ansi, WinAttr };

constexpr const char* LEVEL_NAME[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};

constexpr const char* LEVEL_ANSI[] = {
    "\x1b[90m",   // trace: gray
    "\x1b[36m",   // debug: cyan
    "\x1b[32m",   // info:  green
    "\x1b[33m",   // warn:  yellow
    "\x1b[31m",   // error: red
    "\x1b[1;41m", // fatal: bold on red
};
constexpr const char* ANSI_DIM = "\x1b[2m";
constexpr const char* ANSI_RESET = "\x1b[0m";

#ifdef _WIN32
constexpr WORD LEVEL_ATTR[] = {
    FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_BLUE,
    FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_INTENSITY,
    BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
};
#endif

struct Stream {
    std::FILE* file = nullptr;
    bool terminal = false;            // attached to an interactive terminal
    Backend native = Backend::None;   // what the terminal supports
#ifdef _WIN32
    HANDLE handle = nullptr;
    WORD default_attr = 0;
#endif
};

bool env_set(const char* name) {
    const char* v = std::getenv(name);
    return v && *v && std::strcmp(v, "0") != 0;
}

void init_stream(Stream& s, std::FILE* file) {
    s.file = file;
#ifdef _WIN32
    s.handle = GetStdHandle(file == stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (s.handle == nullptr || s.handle == INVALID_HANDLE_VALUE || !GetConsoleMode(s.handle, &mode))
        return; // redirected to file/pipe

    s.terminal = true;
    if (SetConsoleMode(s.handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        s.native = Backend::Ansi; // Windows 10+
        return;
    }
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(s.handle, &info)) {
        s.default_attr = info.wAttributes;
        s.native = Backend::WinAttr; // legacy console
    }
#else
    const char* term = std::getenv("TERM");
    s.terminal = isatty(fileno(file)) && !(term && std::strcmp(term, "dumb") == 0);
    s.native = Backend::Ansi;
#endif
}

struct State {
    std::mutex mutex;
    Stream out, err;
    bool no_color = env_set("NO_COLOR");
    bool force_color = env_set("FORCE_COLOR") || env_set("CLICOLOR_FORCE");

    State() {
        init_stream(out, stdout);
        init_stream(err, stderr);
    }
};

State& state() {
    static State s;
    return s;
}

std::atomic<Level> g_level{Level::Info};
std::atomic<ColorMode> g_color_mode{ColorMode::Auto};

Backend pick_backend(const State& st, const Stream& s) {
    switch (g_color_mode.load(std::memory_order_relaxed)) {
        case ColorMode::Never:
            return Backend::None;
        case ColorMode::Always:
            return s.terminal && s.native != Backend::None ? s.native : Backend::Ansi;
        case ColorMode::Auto:
        default:
            if (st.no_color) return Backend::None;
            if (s.terminal) return s.native;
            return st.force_color ? Backend::Ansi : Backend::None;
    }
}

void format_time(char (&buf)[16]) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms));
}

} // namespace

void set_level(Level lvl) { g_level.store(lvl, std::memory_order_relaxed); }

Level level() { return g_level.load(std::memory_order_relaxed); }

void set_color_mode(ColorMode mode) { g_color_mode.store(mode, std::memory_order_relaxed); }

void write(Level lvl, std::string_view message) {
    if (!enabled(lvl)) return;

    const auto idx = static_cast<std::size_t>(lvl);
    char time[16];
    format_time(time);

    State& st = state();
    Stream& s = lvl >= Level::Warn ? st.err : st.out;

    std::lock_guard lock(st.mutex);
    std::FILE* f = s.file;

    switch (pick_backend(st, s)) {
        case Backend::Ansi:
            std::fprintf(f, "%s%s%s %s%s%s ", ANSI_DIM, time, ANSI_RESET,
                         LEVEL_ANSI[idx], LEVEL_NAME[idx], ANSI_RESET);
            break;
#ifdef _WIN32
        case Backend::WinAttr:
            std::fprintf(f, "%s ", time);
            std::fflush(f);
            SetConsoleTextAttribute(s.handle, LEVEL_ATTR[idx]);
            std::fputs(LEVEL_NAME[idx], f);
            std::fflush(f);
            SetConsoleTextAttribute(s.handle, s.default_attr);
            std::fputc(' ', f);
            break;
#endif
        default:
            std::fprintf(f, "%s %s ", time, LEVEL_NAME[idx]);
            break;
    }

    std::fwrite(message.data(), 1, message.size(), f);
    std::fputc('\n', f);
    std::fflush(f);
}

} // namespace raptor::log
