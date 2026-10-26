import shutil
import os
import subprocess
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which("g++") or shutil.which("c++")


@unittest.skipUnless(COMPILER, "C++17 compiler not installed")
class SignalTests(unittest.TestCase):
    def test_catalogue_mapping_raster_phase_and_impact_recovery(self):
        source = r'''#include "sprites.hpp"
#include <cassert>
#include <cmath>
#include <set>
int main() {
    using namespace course;
    const std::set<std::size_t> alternate{3, 4, 5, 12, 13};
    for (std::size_t i = 0; i < art::signal_metadata.size(); ++i) {
        const auto profile = art::signal_profile(i);
        assert(std::isfinite(profile.x_frequency) && profile.x_frequency > 0.0);
        assert(std::isfinite(profile.y_frequency) && profile.y_frequency > 0.0);
        assert(profile.alternate == (alternate.count(i) != 0));
        assert(art::lissajous(profile, 0.0, 1.0, 29, 5) !=
               art::lissajous(profile, 0.7, 1.0, 29, 5));
        const auto braille = art::lissajous(profile, 0.3, 1.0, 29, 5, true);
        bool has_subcell = false;
        for (const auto& row : braille) for (unsigned char byte : row) has_subcell |= byte >= 0xe0;
        assert(has_subcell);
    }
    assert(*art::signal_metadata[0].mass_mev == 0.51099895069);
    assert(!art::signal_metadata[3].mass_mev && art::signal_metadata[12].mass_mev == 0.0);
    assert(art::signal_metadata[15].mass_mev && !art::signal_profile(15).alternate);
    assert(art::signal_metadata[16].spin == 0.0);
    art::SignalMotion motion;
    motion.impact();
    motion.advance(0.20);
    assert(std::abs(motion.gain - 0.10) < 1e-12);
    assert(std::abs(motion.phase - 0.02) < 1e-12);
    motion.advance(0.10);
    assert(std::abs(motion.gain - 0.10) < 1e-12);
    for (int i = 0; i < 6; ++i) motion.advance(0.25);
    motion.advance(0.20);
    assert(std::abs(motion.gain - 1.0) < 1e-12);
    motion.impact();
    assert(std::abs(motion.gain - 0.10) < 1e-12);
    art::SignalMotion unwrapped;
    for (int i = 0; i < 26; ++i) unwrapped.advance(0.25);
    assert(unwrapped.phase > 6.283185307179586);
    const auto profile = art::signal_profile(0);
    const auto frame_before = art::lissajous(profile, 6.27, 1.0, 29, 5);
    const auto frame_after = art::lissajous(profile, 6.29, 1.0, 29, 5);
    int changed_cells = 0;
    for (std::size_t y = 0; y < frame_before.size(); ++y)
        for (std::size_t x = 0; x < frame_before[y].size(); ++x)
            changed_cells += frame_before[y][x] != frame_after[y][x];
    assert(changed_cells < 29 * 5 / 3);  // no whole-curve jump at 2*pi
    const double before = std::sin(profile.x_frequency * 0.5 + 6.27 + profile.phase);
    const double after = std::sin(profile.x_frequency * 0.5 + 6.29 + profile.phase);
    assert(std::abs(after - before) < 0.1);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / "signals.cpp"
            binary = Path(folder) / ("signals.exe" if os.name == "nt" else "signals")
            cpp.write_text(source)
            built = subprocess.run([COMPILER, "-std=c++17", "-I", str(ROOT / "engine"),
                                    str(cpp), "-o", str(binary)],
                                   text=True, capture_output=True, timeout=30)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=5)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
