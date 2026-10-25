#pragma once
#include "domain.hpp"
#include <csignal>
#include <iostream>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#else
#include <cerrno>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace course {
inline volatile std::sig_atomic_t interrupted = 0;
inline void on_interrupt(int) { interrupted = 1; }
class Terminal {
    bool step_, ansi_, sound_;
#ifdef _WIN32
    HANDLE output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD old_mode_ = 0;
    bool changed_ = false;
#else
    termios old_{};
    bool changed_ = false;
#endif
public:
    Terminal(bool step, bool plain, bool sound) : step_(step), ansi_(!plain), sound_(sound) {
#ifdef _WIN32
        DWORD mode;
        if (!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)) step_ = true;
        if (GetConsoleMode(output_, &old_mode_)) {
            changed_ = SetConsoleMode(output_, old_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            if (!changed_) ansi_ = false;
        } else ansi_ = false;
#else
        if (!isatty(STDIN_FILENO)) step_ = true;
        if (!isatty(STDOUT_FILENO)) ansi_ = false;
        if (!step_ && tcgetattr(STDIN_FILENO, &old_) == 0) {
            auto raw = old_;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 1;
            changed_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
            if (!changed_) step_ = true;
        }
#endif
        std::signal(SIGINT, on_interrupt);
        std::signal(SIGTERM, on_interrupt);
        if (ansi_) std::cout << "\033[?25l";
    }
    ~Terminal() {
#ifdef _WIN32
        if (changed_) SetConsoleMode(output_, old_mode_);
#else
        if (changed_) tcsetattr(STDIN_FILENO, TCSANOW, &old_);
#endif
        if (ansi_) std::cout << "\033[0m\033[?25h\n" << std::flush;
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
        std::cout << '\a' << std::flush; // Campana del terminal, configurable por el usuario.
#endif
    }
    int key() const {
        if (interrupted) return 'q';
        if (step_) {
            std::cout << "\naccion> " << std::flush;
            std::string line;
            return std::getline(std::cin, line) && !line.empty() ? line[0] : 'q';
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
            if (ready > 0 && read(STDIN_FILENO, &c, 1) == 1) break;
        }
        if (interrupted) return 'q';
        if (c == '\033') {
            char seq[2]{};
            // termios VTIME limita esta lectura; ESC solo no bloquea la interfaz.
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[' && read(STDIN_FILENO, &seq[1], 1) == 1)
                return seq[1] == 'A' ? 'w' : seq[1] == 'B' ? 's' : seq[1] == 'C' ? 'd' : seq[1] == 'D' ? 'a' : 0;
            return 0;
        }
        return static_cast<unsigned char>(c);
#endif
    }
};
} // namespace course
