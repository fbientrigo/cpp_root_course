#pragma once
#include "domain.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#else
#include <cerrno>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace course {
inline volatile std::sig_atomic_t interrupted = 0;
inline void on_interrupt(int) { interrupted = 1; }
class Terminal {
public:
    struct Dimensions { int columns = 80; int rows = 24; };
private:
    bool step_, ansi_ = false, sound_, unicode_ = false;
    std::string previous_frame_;
#ifdef _WIN32
    HANDLE output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD old_mode_ = 0;
    UINT old_codepage_ = 0;
    bool changed_ = false;
    using SignalHandler = void (*)(int);
    SignalHandler old_int_ = SIG_DFL, old_term_ = SIG_DFL;
#else
    termios old_{};
    bool changed_ = false;
    struct sigaction old_int_{}, old_term_{};
#endif
    // Renderer glyphs have one display cell. Preserve CSI colors while clipping UTF-8
    // at character boundaries; reserve the last column to avoid terminal autowrap.
    static std::string clipped(const std::string& row, int width) {
        std::string result;
        int cells = 0;
        for (std::size_t i = 0; i < row.size() && cells < width;) {
            const auto c = static_cast<unsigned char>(row[i]);
            if (c == 27 && i + 1 < row.size() && row[i + 1] == '[') {
                const auto begin = i;
                i += 2;
                while (i < row.size()) {
                    const auto end = static_cast<unsigned char>(row[i++]);
                    if (end >= 0x40 && end <= 0x7e) break;
                }
                result.append(row, begin, i - begin);
            } else if (c < 32 || c == 127) {
                ++i; // A frame cannot inject newlines, tabs or other terminal controls.
            } else {
                const std::size_t n = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
                if (i + n > row.size()) break;
                result.append(row, i, n);
                i += n;
                ++cells;
            }
        }
        return result;
    }
public:
    Terminal(bool step, bool plain, bool sound) : step_(step), sound_(sound) {
        interrupted = 0;
#ifdef _WIN32
        DWORD input_mode;
        const bool input_tty = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &input_mode) != 0;
        if (!input_tty) step_ = true;
        if (!plain && !step_ && GetConsoleMode(output_, &old_mode_)) {
            changed_ = SetConsoleMode(output_, old_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            ansi_ = changed_;
        }
        if (ansi_) {
            old_codepage_ = GetConsoleOutputCP();
            unicode_ = SetConsoleOutputCP(CP_UTF8) != 0;
        }
        old_int_ = std::signal(SIGINT, on_interrupt);
        old_term_ = std::signal(SIGTERM, on_interrupt);
#else
        const bool input_tty = isatty(STDIN_FILENO) != 0;
        if (!input_tty) step_ = true;
        const char* term = std::getenv("TERM");
        ansi_ = !plain && !step_ && isatty(STDOUT_FILENO) && term && *term && std::string(term) != "dumb";
        if (!step_ && tcgetattr(STDIN_FILENO, &old_) == 0) {
            auto raw = old_;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 1;
            changed_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
            if (!changed_) { step_ = true; ansi_ = false; }
        }
        const char* locale = std::getenv("LC_ALL");
        if (!locale || !*locale) locale = std::getenv("LC_CTYPE");
        if (!locale || !*locale) locale = std::getenv("LANG");
        if (locale) {
            const std::string name(locale);
            unicode_ = name.find("UTF-8") != std::string::npos || name.find("utf8") != std::string::npos || name.find("utf-8") != std::string::npos;
        }
        struct sigaction action{};
        action.sa_handler = on_interrupt;
        sigemptyset(&action.sa_mask);
        // No SA_RESTART: signals must also release canonical (--step) input.
        sigaction(SIGINT, &action, &old_int_);
        sigaction(SIGTERM, &action, &old_term_);
#endif
        if (ansi_) std::cout << "\033[?1049h\033[?25l\033[0;37;40m\033[H\033[2J" << std::flush;
    }
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    ~Terminal() {
        // Restore the visible screen while VT processing is still enabled.
        if (ansi_) std::cout << "\033[0m\033[?25h\033[?1049l" << std::flush;
#ifdef _WIN32
        if (old_codepage_) SetConsoleOutputCP(old_codepage_);
        if (changed_) SetConsoleMode(output_, old_mode_);
        std::signal(SIGINT, old_int_);
        std::signal(SIGTERM, old_term_);
#else
        if (changed_) tcsetattr(STDIN_FILENO, TCSANOW, &old_);
        sigaction(SIGINT, &old_int_, nullptr);
        sigaction(SIGTERM, &old_term_, nullptr);
#endif
    }
    bool rich() const { return ansi_; }
    bool unicode() const { return ansi_ && unicode_; }
    Dimensions dimensions() const {
        Dimensions size;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(output_, &info)) {
            size.columns = info.srWindow.Right - info.srWindow.Left + 1;
            size.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        }
#else
        winsize info{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &info) == 0) {
            if (info.ws_col) size.columns = info.ws_col;
            if (info.ws_row) size.rows = info.ws_row;
        }
#endif
        return size;
    }
    void present(const std::vector<std::string>& rows) {
        if (!ansi_) {
            for (const auto& row : rows) std::cout << row << '\n';
            std::cout << std::flush;
            return;
        }
        const auto size = dimensions();
        const int count = std::min(static_cast<int>(rows.size()), size.rows);
        std::string frame = "\033[H";
        for (int i = 0; i < count; ++i) {
            frame += "\033[" + std::to_string(i + 1) + ";1H\033[0;37;40m";
            frame += clipped(rows[static_cast<std::size_t>(i)], std::max(0, size.columns - 1));
            frame += "\033[0;37;40m\033[K";
        }
        if (count < size.rows) frame += "\033[" + std::to_string(count + 1) + ";1H\033[0;37;40m\033[J";
        // Include dimensions: a resize must repaint even if the text is unchanged.
        const std::string identity = std::to_string(size.columns) + "x" + std::to_string(size.rows) + frame;
        if (identity != previous_frame_) {
            std::cout << frame << std::flush;
            previous_frame_ = identity;
        }
    }
    void clear() const { if (ansi_) std::cout << "\033[2J\033[H"; }
    void ink(const char* text) const {
        if (ansi_) std::cout << "\033[36m";
        std::cout << text;
        if (ansi_) std::cout << "\033[0m";
    }
    void sound() const {
        if (!sound_) return;
#ifdef _WIN32
        Beep(660, 60); Beep(880, 80);
#else
        std::cout << '\a' << std::flush;
#endif
    }
    int key() const {
        if (interrupted) return 'q';
        if (step_) {
            std::cout << "\naccion> " << std::flush;
#ifdef _WIN32
            std::string line;
            return std::getline(std::cin, line) && !line.empty() ? line[0] : 'q';
#else
            // read(), unlike an iostream with restarted stdio, can leave on SIGINT.
            std::string line;
            while (!interrupted) {
                char c;
                const auto n = read(STDIN_FILENO, &c, 1);
                if (n == 0 || (n < 0 && errno != EINTR)) return 'q';
                if (n < 0) continue;
                if (c == '\n') return line.empty() ? 'q' : static_cast<unsigned char>(line[0]);
                line += c;
            }
            return 'q';
#endif
        }
#ifdef _WIN32
        while (!_kbhit()) { if (interrupted) return 'q'; Sleep(20); }
        int c = _getch();
        if (c == 0 || c == 224) {
            c = _getch();
            return c == 72 ? 'w' : c == 80 ? 's' : c == 75 ? 'a' : c == 77 ? 'd' : 0;
        }
        return c;
#else
        char c = 0;
        while (!interrupted) {
            fd_set set; FD_ZERO(&set); FD_SET(STDIN_FILENO, &set);
            timeval timeout{0, 100000};
            const int ready = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);
            if (ready < 0 && errno != EINTR) return 'q';
            if (ready > 0) {
                const auto n = read(STDIN_FILENO, &c, 1);
                if (n == 1) break;
                if (n == 0 || (n < 0 && errno != EINTR)) return 'q';
            }
        }
        if (interrupted) return 'q';
        if (c == '\033') {
            char seq[2]{};
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[' && read(STDIN_FILENO, &seq[1], 1) == 1)
                return seq[1] == 'A' ? 'w' : seq[1] == 'B' ? 's' : seq[1] == 'C' ? 'd' : seq[1] == 'D' ? 'a' : 0;
            return 0;
        }
        return static_cast<unsigned char>(c);
#endif
    }
};
} // namespace course
