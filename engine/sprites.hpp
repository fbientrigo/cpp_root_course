#pragma once
#include "domain.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace course::art {
struct SignalMetadata {
    int pdg_id;
    std::optional<double> mass_mev;
    double spin;
};

// PDG 2026 Particle Listings tables (MeV/c^2):
// https://pdg.lbl.gov/2026/listings/particle_properties.html
// Quark schemes: https://pdg.lbl.gov/2026/reviews/rpp2026-rev-quark-masses.pdf
// Top reconstruction: https://pdg.lbl.gov/2026/reviews/rpp2026-rev-top-quark.pdf
// MC IDs: https://pdg.lbl.gov/2026/reviews/rpp2026-rev-monte-carlo-numbering.pdf
// Quark masses use their stated schemes:
// u/d/s in MS-bar at 2 GeV, c/b in MS-bar at their own mass, t from direct
// reconstruction. Photon/gluon use the Standard Model m=0 assumption; the
// neutrino flavour states have no single point mass. These are visual inputs.
inline constexpr std::array<SignalMetadata, 17> signal_metadata{{
    {11, 0.51099895069, .5}, {13, 105.6583755, .5}, {15, 1776.93, .5},
    {12, std::nullopt, .5}, {14, std::nullopt, .5}, {16, std::nullopt, .5},
    {2, 2.16, .5}, {1, 4.70, .5}, {4, 1272.9, .5}, {3, 92.9, .5},
    {6, 172600.0, .5}, {5, 4186.0, .5}, {22, 0.0, 1.0}, {21, 0.0, 1.0},
    {24, 80362.5, 1.0}, {23, 91187.9, 1.0}, {25, 125130.0, 0.0}
}};

struct SignalProfile {
    double x_frequency = 1.0, y_frequency = 1.0, phase = 0.0;
    bool alternate = false;
};

inline SignalProfile signal_profile(std::size_t species) {
    const auto& metadata = signal_metadata.at(species);
    const double charge = catalogue().at(species).charge3 / 3.0;
    if (metadata.mass_mev && *metadata.mass_mev > 0.0) {
        constexpr double electron_mass_mev = 0.51099895069;
        const double ratio = *metadata.mass_mev / electron_mass_mev;
        const double x = 1.0 + std::log10(1.0 + ratio);
        const double y = 1.0 + std::log10(1.0 + std::abs(charge) / ratio);
        return {x, y, charge > 0 ? 1.5707963267948966 : charge < 0 ? -1.5707963267948966 : 0.0, false};
    }
    // Finite charge/spin/PDG-derived fallback for m=0 and undefined mass.
    return {1.0 + 2.0 * metadata.spin + std::abs(charge),
            1.0 + 1.4142135623730951 * (1.0 + std::abs(charge)),
            0.2 * (std::abs(metadata.pdg_id) % 7), true};
}

struct SignalMotion {
    double phase = 0.0;
    double gain = 1.0;
    double impact_time = 10.0;

    void impact() { gain = 0.10; impact_time = 0.0; }
    void advance(double seconds) {
        const double dt = std::clamp(seconds, 0.0, 0.25);
        impact_time += dt;
        // Hold the impact response briefly, then smoothly restore full speed.
        if (impact_time <= 0.30) gain = 0.10;
        else {
            const double t = std::clamp((impact_time - 0.30) / 1.70, 0.0, 1.0);
            const double smooth = t * t * (3.0 - 2.0 * t);
            gain = 0.10 + 0.90 * smooth;
        }
        // Keep the shared phase unwrapped. The Y oscillator is incommensurate,
        // so wrapping it at 2*pi would create a visible discontinuity.
        phase += dt * gain;
    }
};

inline std::vector<std::string> lissajous(const SignalProfile& profile,
                                          double phase, double gain,
                                          int width, int height = 7,
                                          bool unicode = false) {
    width = std::max(9, width);
    height = std::max(3, height);
    const int pixel_width = unicode ? width * 2 : width;
    const int pixel_height = unicode ? height * 4 : height;
    std::vector<std::string> grid(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' '));
    std::vector<std::vector<unsigned char>> dots;
    if (unicode) dots.assign(static_cast<std::size_t>(height),
                             std::vector<unsigned char>(static_cast<std::size_t>(width), 0));
    const double pi2 = 6.283185307179586;
    const int samples = std::max(unicode ? 1800 : 180, pixel_width * pixel_height * (unicode ? 4 : 6));
    const double phase_x = phase + profile.phase;
    const double phase_y = phase * 1.4142135623730951;
    for (int i = 0; i <= samples; ++i) {
        const double t = pi2 * i / samples;
        const double x = (std::sin(profile.x_frequency * gain * t + phase_x) + 1.0) * 0.5;
        const double y = (std::sin(profile.y_frequency * gain * t + phase_y) + 1.0) * 0.5;
        const int px = std::clamp(static_cast<int>(std::lround(x * (pixel_width - 1))), 0, pixel_width - 1);
        const int py = std::clamp(static_cast<int>(std::lround((1.0 - y) * (pixel_height - 1))), 0, pixel_height - 1);
        if (!unicode) grid[static_cast<std::size_t>(py)][static_cast<std::size_t>(px)] = '*';
        else {
            const int cell_x = px / 2, cell_y = py / 4;
            const int local_x = px % 2, local_y = py % 4;
            static constexpr unsigned char braille_bits[4][2] = {{0x01, 0x08}, {0x02, 0x10},
                                                                  {0x04, 0x20}, {0x40, 0x80}};
            dots[static_cast<std::size_t>(cell_y)][static_cast<std::size_t>(cell_x)] |= braille_bits[local_y][local_x];
        }
    }
    if (unicode) {
        std::vector<std::string> rows(static_cast<std::size_t>(height));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int codepoint = dots[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]
                    ? 0x2800 + dots[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] : 0x20;
                if (codepoint == 0x20) rows[static_cast<std::size_t>(y)] += ' ';
                else {
                    rows[static_cast<std::size_t>(y)] += static_cast<char>(0xe0 | (codepoint >> 12));
                    rows[static_cast<std::size_t>(y)] += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
                    rows[static_cast<std::size_t>(y)] += static_cast<char>(0x80 | (codepoint & 0x3f));
                }
            }
        }
        return rows;
    }
    return grid;
}
} // namespace course::art
