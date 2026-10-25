#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace course::art {
// Original fictional mascots, not models of elementary-particle geometry.
// One geometric shape per family, eyes punched out, one or two colors.
// Each letter is one square pixel: '.' background, 'b' body, 'c' accent.
// Dots above the shape count the generation, the same rule for every family.
constexpr int width = 16, height = 12;
using Pixels = std::array<std::string_view, height>;
struct Palette { int body; int accent; };

inline constexpr Pixels circle = {{ // charged leptons
    "................",
    "................",
    "......bbbb......",
    ".....bbbbbb.....",
    "....bbbbbbbb....",
    "...bbbbbbbbbb...",
    "...bbb.bb.bbb...",
    "...bbb.bb.bbb...",
    "...bbbbbbbbbb...",
    "....bbbbbbbb....",
    ".....bbbbbb.....",
    "......bbbb......"
}};

inline constexpr Pixels ring = {{ // neutrinos: almost nothing there
    "................",
    "................",
    "......bbbb......",
    ".....bbbbbb.....",
    "....bb....bb....",
    "...bb......bb...",
    "...bb.c..c.bb...",
    "...bb.c..c.bb...",
    "...bb......bb...",
    "....bb....bb....",
    ".....bbbbbb.....",
    "......bbbb......"
}};

inline constexpr Pixels square = {{ // quarks
    "................",
    "................",
    "................",
    "...bbbbbbbbbb...",
    "...bbbbbbbbbb...",
    "...bbbbbbbbbb...",
    "...bbb.bb.bbb...",
    "...bbb.bb.bbb...",
    "...bbbbbbbbbb...",
    "...bbbbbbbbbb...",
    "...bbbbbbbbbb...",
    "................"
}};

inline constexpr Pixels cross = {{ // photon and gluon
    "................",
    "................",
    "......bbbb......",
    "......bbbb......",
    "......bbbb......",
    "..bbbbbbbbbbbb..",
    "..bbbb.bb.bbbb..",
    "..bbbb.bb.bbbb..",
    "..bbbbbbbbbbbb..",
    "......bbbb......",
    "......bbbb......",
    "......bbbb......"
}};

inline constexpr Pixels diamond = {{ // W and Z
    "................",
    "................",
    ".......bb.......",
    "......bbbb......",
    ".....bbbbbb.....",
    "....bbbbbbbb....",
    "...bbb.bb.bbb...",
    "...bbb.bb.bbb...",
    "....bbbbbbbb....",
    ".....bbbbbb.....",
    "......bbbb......",
    ".......bb......."
}};

inline constexpr Pixels triangle = {{ // Higgs
    "................",
    "................",
    ".......bb.......",
    "......bbbb......",
    "......bbbb......",
    ".....bbbbbb.....",
    "....bb.bb.bb....",
    "....bb.bb.bb....",
    "...bbbbbbbbbb...",
    "..bbbbbbbbbbbb..",
    "..bbbbbbbbbbbb..",
    "................"
}};

inline std::string color(int index, bool background) {
    return "\033[" + std::to_string((background ? 40 : 30) + index % 8 + (index >= 8 ? 60 : 0)) + "m";
}

// Two vertical pixels per cell with half blocks: height / 2 rows of width cells.
inline std::vector<std::string> raster(const Pixels& pixels, Palette palette) {
    std::vector<std::string> rows;
    auto pixel = [&](int x, int y) {
        const char p = pixels[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        return p == 'b' ? palette.body : p == 'c' ? palette.accent : 0;
    };
    for (int y = 0; y < height; y += 2) {
        std::string row;
        for (int x = 0; x < width; ++x) {
            const int top = pixel(x, y), bottom = pixel(x, y + 1);
            if (top == bottom) row += color(top, false) + color(0, true) + (top ? "█" : " ");
            else if (!top) row += color(bottom, false) + color(0, true) + "▄";
            else row += color(top, false) + color(bottom, true) + "▀";
        }
        rows.push_back(row + "\033[0;37;40m");
    }
    return rows;
}

inline std::vector<std::string> sprite(std::size_t species) {
    // Catalogue order from domain.hpp.
    static constexpr std::string_view generation[] = {
        "................", ".......cc.......", ".....cc..cc....."};
    Pixels pixels = circle;
    Palette palette{14, 14};
    int gen = 0;
    if (species <= 2) {
        const int body[] = {14, 13, 12};
        gen = static_cast<int>(species);
        palette = {body[species], body[species]};
    } else if (species <= 5) {
        // The eyes borrow the color of the charged partner.
        const int partner[] = {14, 13, 12};
        pixels = ring; gen = static_cast<int>(species) - 3;
        palette = {7, partner[species - 3]};
    } else if (species <= 11) {
        // Up-type warm, down-type cool.
        pixels = square; gen = static_cast<int>(species - 6) / 2;
        const int body = species % 2 == 0 ? 11 : 10;
        palette = {body, body};
    } else if (species <= 13) {
        pixels = cross; palette = species == 12 ? Palette{15, 15} : Palette{10, 10};
    } else if (species <= 15) {
        pixels = diamond; palette = species == 14 ? Palette{12, 12} : Palette{6, 6};
    } else {
        pixels = triangle; palette = {3, 3};
    }
    pixels[0] = generation[gen];
    return raster(pixels, palette);
}
} // namespace course::art
