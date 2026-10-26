import csv
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata
import unittest
import zipfile

if os.name != "nt":
    import fcntl
    import pty
    import select
    import signal
    import struct
    import termios

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("course_cli", ROOT / "course.py")
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class TerminalScreen:
    """Small VT screen model: detect scrolling and writes outside the viewport."""
    def __init__(self, columns, rows):
        self.columns, self.rows = columns, rows
        self.x = self.y = self.scrolls = self.overflow = 0
        self.active = False
        self.cells = [[" "] * columns for _ in range(rows)]

    def read(self, output):
        for token in re.findall(r"\x1b\[[0-?]*[ -/]*[@-~]|[^\x1b]", output.decode("utf-8")):
            if token.startswith("\x1b["):
                args, command = token[2:-1], token[-1]
                if args == "?1049" and command in "hl":
                    self.active = command == "h"
                    self.x = self.y = 0
                elif self.active:
                    values = [int(v or "0") for v in args.split(";")] if not args.startswith("?") else []
                    if command in "Hf":
                        self.y = max(0, (values[0] if values else 1) - 1)
                        self.x = max(0, (values[1] if len(values) > 1 else 1) - 1)
                        self.overflow += self.y >= self.rows or self.x >= self.columns
                    elif command == "J" and values and values[0] == 2:
                        self.cells = [[" "] * self.columns for _ in range(self.rows)]
                    elif command == "J" and (not values or values[0] == 0):
                        if self.y < self.rows:
                            self.cells[self.y][self.x:] = [" "] * max(0, self.columns - self.x)
                            for row in range(self.y + 1, self.rows):
                                self.cells[row] = [" "] * self.columns
                    elif command == "K" and self.y < self.rows:
                        start = 0 if values and values[0] == 2 else self.x
                        self.cells[self.y][start:] = [" "] * max(0, self.columns - start)
                continue
            if not self.active:
                continue
            if token == "\r":
                self.x = 0
            elif token == "\n":
                self.y += 1
                if self.y >= self.rows:
                    self.scrolls += 1
                    self.y = self.rows - 1
            elif token >= " " and token != "\x7f":
                width = 0 if unicodedata.combining(token) else (2 if unicodedata.east_asian_width(token) in "WF" else 1)
                if self.x + width > self.columns:
                    self.x = 0
                    self.y += 1
                    if self.y >= self.rows:
                        self.scrolls += 1
                        self.y = self.rows - 1
                if self.y < self.rows and self.x < self.columns:
                    self.cells[self.y][self.x] = token
                else:
                    self.overflow += 1
                self.x += width
        return "\n".join("".join(row) for row in self.cells)


class PtyGame:
    def __init__(self, binary, folder, *args, columns=80, rows=24, term="xterm-256color", colorterm=None,
                 locale="C.UTF-8", demo=True):
        self.master, self.slave = pty.openpty()
        self.resize(columns, rows)
        self.before = termios.tcgetattr(self.slave)
        self.output = b""
        env = dict(os.environ, TERM=term, LANG=locale, LC_ALL=locale)
        env.pop("COLORTERM", None)
        if colorterm:
            env["COLORTERM"] = colorterm
        command = [str(binary)] + (["--demo"] if demo else []) + list(args)
        self.process = subprocess.Popen(command, stdin=self.slave,
                                        stdout=self.slave, stderr=self.slave, cwd=folder,
                                        env=env)

    def resize(self, columns, rows):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))

    def read(self, timeout=.05):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.master], [], [], max(0, deadline - time.monotonic()))
            if not ready:
                break
            try:
                data = os.read(self.master, 65536)
            except OSError:
                break
            if not data:
                break
            self.output += data
        return self.output

    def wait_for(self, marker, start=0):
        deadline = time.monotonic() + 5
        while marker not in self.output[start:] and time.monotonic() < deadline:
            self.read(.05)
        if marker not in self.output[start:]:
            raise AssertionError("PTY did not emit %r: %r" % (marker, self.output[-1000:]))

    def key(self, key):
        os.write(self.master, key.encode())
        self.read(.02)

    def finish(self, sig=None):
        if sig is None:
            os.write(self.master, b"q")
        else:
            self.process.send_signal(sig)
        self.process.wait(timeout=5)
        self.read(.05)

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        os.close(self.master)
        os.close(self.slave)


class CourseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binaries = {i: cli.build(i) for i in range(1, 7)}

    def run_game(self, level, *args, input=None, cwd=None):
        return subprocess.run([str(self.binaries[level]), *args], cwd=cwd or ROOT,
                              input=input, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)

    def test_all_reference_checks_and_unfinished_workshops(self):
        with tempfile.TemporaryDirectory() as folder:
            for level in range(1, 7):
                with self.subTest(level=level):
                    demo = self.run_game(level, "--check", "--demo", cwd=folder)
                    self.assertEqual(demo.returncode, 0, demo.stdout + demo.stderr)
                    workshop = self.run_game(level, "--check", "--student", "test", cwd=folder)
                    self.assertEqual(workshop.returncode, 1, workshop.stdout + workshop.stderr)
            self.assertEqual(json.loads((Path(folder) / ".course/demo/progress.json").read_text())["achievements"], [])
            self.assertEqual(json.loads((Path(folder) / ".course/test/progress.json").read_text())["achievements"], [])

    def test_batch_kinematics_selection_and_repeatability(self):
        with tempfile.TemporaryDirectory() as folder:
            args = ("--demo", "--batch", "200", "--seed", "42")
            first = self.run_game(4, *args, cwd=folder)
            self.assertEqual(first.returncode, 0, first.stderr)
            path = Path(folder) / "output/events-s04.csv"
            content = path.read_bytes()
            self.assertEqual(self.run_game(4, *args, cwd=folder).returncode, 0)
            self.assertEqual(content, path.read_bytes())
            rows = list(csv.DictReader(io.StringIO(content.decode())))
            self.assertEqual(len(rows), 200)
            for row in rows:
                r = {k: float(v) for k, v in row.items()}
                for j in (1, 2):
                    mass2 = r['E%d' % j] ** 2 - sum(r['%s%d' % (axis, j)] ** 2 for axis in ('px', 'py', 'pz'))
                    self.assertAlmostEqual(mass2, 0.1056584 ** 2, places=10)
                expected = (r['q1'] * r['q2'] < 0 and math.hypot(r['px1'], r['py1']) >= .6 and math.hypot(r['px2'], r['py2']) >= .6)
                self.assertEqual(bool(r['selected']), expected)

    def test_pending_writer_does_not_replace_existing_data(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "output/events-s05.csv"
            path.parent.mkdir(); path.write_text("previous data")
            result = self.run_game(5, "--batch", "20", cwd=folder)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(path.read_text(), "previous data")

    def test_tui_encounter_capture_export_and_demo_isolation(self):
        with tempfile.TemporaryDirectory() as folder:
            result = self.run_game(3, "--demo", "--plain", "--step", "--student", "ana",
                                   input="e\n1\n1\n2\nx\nq\n", cwd=folder)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Especimen incorporado al registro", result.stdout)
            self.assertIn("PARTICLE_REGISTRY 2/6", result.stdout)
            self.assertIn("SPECIMEN_INSPECTOR", result.stdout)
            self.assertIn("EVENT_INSPECTOR [DATOS SINTETICOS]", result.stdout)
            self.assertEqual(len(list(csv.DictReader(io.StringIO((Path(folder) / "output/encounters.csv").read_text())))), 1)
            profile = json.loads((Path(folder) / ".course/demo/progress.json").read_text())
            self.assertTrue(profile['demo']); self.assertEqual(profile['achievements'], [])
            self.assertFalse((Path(folder) / ".course/ana").exists())

    def test_invalid_arguments_and_profile_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            for args in [("--student", "../escape"), ("--batch", "-1"), ("--batch", "3bad"),
                         ("--seed", "-1"), ("--unknown",)]:
                with self.subTest(args=args):
                    result = self.run_game(1, *args, cwd=folder)
                    self.assertEqual(result.returncode, 2, result.stdout)
            self.assertFalse((Path(folder) / "escape").exists())

    def test_student_change_reaches_game_and_earns_check(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            source = (ROOT / "levels/01/lesson.cpp").read_text().replace(
                "    return false;", "    course::Position next{player.x + input.dx, player.y + input.dy};\n"
                "    if (!world.walkable(next)) return false;\n    player = next;\n    return true;")
            lesson = folder / "lesson.cpp"; lesson.write_text(source)
            binary = folder / ("student.exe" if os.name == "nt" else "student")
            result = subprocess.run([cli.compiler(), "-std=c++17", "-g", "-O0", "-I" + str(ROOT / "engine"),
                                     str(ROOT / "engine/main.cpp"), str(ROOT / "engine/lesson.cpp"), str(lesson),
                                     "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary), "--check", "--student", "ana"], cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            profile = json.loads((folder / ".course/ana/progress.json").read_text())
            self.assertIn("check01", profile['achievements']); self.assertTrue(profile['sessions']['1']['pair'])
            result = subprocess.run([str(binary), "--plain", "--step", "--student", "ana"], cwd=folder,
                                    input="d\nq\n", capture_output=True, text=True)
            self.assertIn("Posicion: (3, 2)", result.stdout)

    @unittest.skipIf(os.name == "nt", "termios solo en POSIX")
    def test_terminal_restores_screen_and_flags_on_exit_and_signals(self):
        with tempfile.TemporaryDirectory() as folder:
            for sig in (None, signal.SIGINT, signal.SIGTERM):
                with self.subTest(signal=sig):
                    game = PtyGame(self.binaries[1], folder)
                    try:
                        game.wait_for(b'[Q]')
                        self.assertIn(b'\x1b[?1049h', game.output)
                        self.assertIn(b'\x1b[?25l', game.output)
                        during = termios.tcgetattr(game.slave)
                        self.assertFalse(during[3] & (termios.ICANON | termios.ECHO))
                        game.finish(sig)
                        self.assertEqual(game.process.returncode, 0)
                        self.assertEqual(termios.tcgetattr(game.slave), game.before)
                        self.assertEqual(game.output.count(b'\x1b[?1049h'), 1)
                        self.assertEqual(game.output.count(b'\x1b[?1049l'), 1)
                        self.assertIn(b'\x1b[?25h', game.output)
                        self.assertIn(b'\x1b[0m', game.output)
                        self.assertNotIn(b'\x1b[3J', game.output)
                    finally:
                        game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_terminal_restores_after_export_exception(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'output').write_text('not a directory')
            game = PtyGame(self.binaries[1], folder)
            try:
                game.wait_for(b'[Q]')
                game.key('x')
                game.process.wait(timeout=5)
                game.read()
                self.assertEqual(game.process.returncode, 2)
                self.assertEqual(termios.tcgetattr(game.slave), game.before)
                self.assertIn(b'\x1b[?1049l', game.output)
                self.assertIn(b'\x1b[?25h', game.output)
                self.assertLess(game.output.index(b'\x1b[?1049l'), game.output.index(b'[ERROR]'))
            finally:
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_persistent_screen_100_moves_and_encounter(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[3], folder, colorterm="truecolor")
            try:
                game.wait_for(b'[Q]')
                game.read(.08)
                unchanged = game.output
                game.key('?')
                self.assertEqual(game.output, unchanged, 'Unchanged state should not repaint')
                for index in range(100):
                    # Close automatic encounters before the next actual movement.
                    game.key('3' + ('a' if index % 2 == 0 else 'd'))
                game.key('3e')
                game.key('1')
                screen = TerminalScreen(80, 24)
                visible = screen.read(game.output)
                self.assertIn('75', visible)
                self.assertIn('damage(100, 25) devolvio 75.', visible)
                self.assertIn('SINTETICO', visible.upper())
                self.assertIn('[Q]', visible)
                self.assertGreaterEqual(game.output.count(b'WIRED / OBSERVATORY'), 100)
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
                game.key('1')
                visible = TerminalScreen(80, 24).read(game.output)
                self.assertIn('Ya puedes intentar [2] Capturar.', visible)
                game.key('2')
                screen = TerminalScreen(80, 24)
                self.assertIn('2/6', screen.read(game.output))
                game.key('v')
                screen = TerminalScreen(80, 24)
                self.assertIn('✓', screen.read(game.output))
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
                game.key('x')
                screen = TerminalScreen(80, 24)
                self.assertIn('CSV:', screen.read(game.output))
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
                self.assertTrue((Path(folder) / 'output/encounters.csv').is_file())
                game.finish()
                self.assertEqual(game.process.returncode, 0)
                self.assertEqual(termios.tcgetattr(game.slave), game.before)
                screen = TerminalScreen(80, 24)
                screen.read(game.output)
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
                self.assertEqual(game.output.count(b'\x1b[?1049h'), 1)
            finally:
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_specimen_signal_keeps_catalogue_and_synthetic_data_distinct(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[3], folder)
            try:
                game.wait_for(b'[Q]')
                screen = TerminalScreen(80, 24)
                visible = screen.read(game.output)
                self.assertIn('TOPOLOGY_OBSERVATORY', visible)
                self.assertIn('PARTICLE_REGISTRY 1/6', visible)
                self.assertIn('[Q] salir', visible.splitlines()[-1])
                self.assertIn('──┤', visible)
                names = {1: 'Muon', 3: 'Neutrino electronico', 6: 'Up',
                         12: 'Foton', 16: 'Higgs', 17: 'Electron'}
                for encounter in range(1, 18):
                    game.key('e')
                    screen = TerminalScreen(80, 24)
                    visible = screen.read(game.output)
                    self.assertIn('SPECIMEN_INSPECTOR', visible)
                    self.assertNotIn('TOPOLOGY_OBSERVATORY', visible)
                    self.assertIn('EVENT_INSPECTOR', visible)
                    self.assertIn('SINTETICO', visible)
                    self.assertIn('100/100', visible)
                    self.assertIn('captura: HP <= 50', visible)
                    self.assertIn('[1] Interactuar', visible)
                    self.assertTrue('*' in visible or any(0x2800 <= ord(char) <= 0x28ff for char in visible))
                    if encounter in names:
                        self.assertIn(names[encounter], visible)
                        self.assertIn('spin=', visible)
                    if encounter in (3, 4, 5, 12, 13):
                        self.assertIn('ALT Q/SPIN', visible)
                        self.assertIn(b'38;5;172', game.output)
                    else:
                        self.assertIn('LOG', visible)
                    if encounter == 6:
                        self.assertIn('q=+2/3 e', visible)
                    if encounter == 1:
                        game.key('1')
                        visible = TerminalScreen(80, 24).read(game.output)
                        self.assertIn('damage(100, 25) devolvio 75.', visible)
                        self.assertIn('base 10%', visible)
                        game.key('1')
                        visible = TerminalScreen(80, 24).read(game.output)
                        self.assertIn('Ya puedes intentar [2] Capturar.', visible)
                    game.key('3')
                # Capture updates the registry without changing its physical labels.
                game.key('e112')
                screen = TerminalScreen(80, 24)
                visible = screen.read(game.output)
                self.assertIn('TOPOLOGY_OBSERVATORY', visible)
                self.assertNotIn('SPECIMEN_INSPECTOR', visible)
                self.assertIn('PARTICLE_REGISTRY 2/6', visible)
                self.assertIn('e- mu-', visible)
                self.assertIn('Ultimo: Muon', visible)
                self.assertIn('[Q] salir', visible.splitlines()[-1])
                for _ in range(4):
                    game.key('e112')
                screen = TerminalScreen(80, 24)
                visible = screen.read(game.output)
                self.assertIn('PARTICLE_REGISTRY 6/6', visible)
                self.assertIn('e- mu- tau- nu_e nu_mu nu_tau', visible)
                self.assertIn('Ultimo: Neutrino tauonico', visible)
                self.assertIn('LEPTON q=0 e HP juego=50', visible)
                self.assertIn('Posicion ', visible)
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
                game.finish()
            finally:
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_long_registry_roster_wraps_without_losing_symbols(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[3], folder)
            try:
                game.wait_for(b"[Q]")
                for _ in range(2):
                    game.key("e")
                    game.key("3")
                game.key("e112")  # electron neutrino
                game.key("e112")  # muon neutrino
                game.key("e112")  # tau neutrino
                for _ in range(14):
                    game.key("e")
                    game.key("3")
                game.key("e112")  # next cycle: electron neutrino
                game.key("e112")  # muon neutrino, sixth registration
                visible = TerminalScreen(80, 24).read(game.output)
                self.assertIn("PARTICLE_REGISTRY 6/6", visible)
                self.assertIn("e- nu_e nu_mu nu_tau nu_e", visible)
                roster_rows = [line for line in visible.splitlines() if "nu_mu" in line]
                self.assertGreaterEqual(len(roster_rows), 2)
                self.assertIn("Ultimo: Neutrino muonico", visible)
                self.assertIn("Posicion ", visible)
                self.assertEqual(TerminalScreen(80, 24).scrolls, 0)
                screen = TerminalScreen(80, 24)
                screen.read(game.output)
                self.assertEqual(screen.overflow, 0)
                game.finish()
            finally:
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_resize_keeps_compact_and_tiny_screens_in_bounds(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[1], folder)
            try:
                game.wait_for(b'[Q]')
                for columns, rows in ((60, 21), (79, 23), (60, 18), (30, 8), (12, 3), (80, 24)):
                    with self.subTest(columns=columns, rows=rows):
                        game.resize(columns, rows)
                        start = len(game.output)
                        game.key('e')
                        game.key('3')
                        # Evaluate only output rendered at these dimensions.
                        screen = TerminalScreen(columns, rows)
                        visible = screen.read(b'\x1b[?1049h' + game.output[start:])
                        self.assertTrue(visible.strip())
                        self.assertEqual(screen.scrolls, 0)
                        self.assertEqual(screen.overflow, 0)
                        if (columns, rows) in ((60, 21), (80, 24)):
                            self.assertIn('[Q] salir', visible.splitlines()[-1])
                game.finish()
                self.assertEqual(termios.tcgetattr(game.slave), game.before)
            finally:
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_pending_encounter_keeps_feedback_and_footer_at_60x21(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[3], folder, columns=60, rows=21, demo=False)
            try:
                game.wait_for(b"[Q]")
                game.key("e")
                game.key("v")
                game.key("1")
                screen = TerminalScreen(60, 21)
                visible = screen.read(game.output)
                self.assertIn("damage(100, 25) devolvio 75.", visible)
                self.assertIn("CODE_INSPECTOR", visible)
                self.assertIn("EVENT_INSPECTOR", visible)
                self.assertIn("m 1.427 GeV", visible)
                self.assertIn("pT 0.585/0.585 GeV", visible)
                self.assertIn("[Q] salir", visible.splitlines()[-1])
                self.assertIn("[1] [2] [3]", visible.splitlines()[-1])
                self.assertEqual(screen.scrolls, 0)
                self.assertEqual(screen.overflow, 0)
            finally:
                game.finish()
                game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_observatory_palette_uses_advertised_terminal_depth(self):
        with tempfile.TemporaryDirectory() as folder:
            for term, colorterm, expected, forbidden in (
                    ("xterm-256color", None, b"38;5;32m", b"38;2;"),
                    ("xterm", "truecolor", b"38;2;2;132;199m", b"38;5;32m"),
                    ("vt100", None, b"\x1b[36m", b"38;")):
                with self.subTest(term=term, colorterm=colorterm):
                    game = PtyGame(self.binaries[3], folder, term=term, colorterm=colorterm)
                    try:
                        game.wait_for(b"[Q]")
                        self.assertIn(expected, game.output)
                        self.assertNotIn(forbidden, game.output)
                    finally:
                        game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_ascii_locale_keeps_topology_and_specimen_ascii(self):
        with tempfile.TemporaryDirectory() as folder:
            game = PtyGame(self.binaries[3], folder, locale="C")
            try:
                game.wait_for(b"[Q]")
                visible = TerminalScreen(80, 24).read(game.output)
                self.assertIn("TOPOLOGY_OBSERVATORY", visible)
                self.assertIn("PARTICLE_REGISTRY", visible)
                game.key("e")
                visible = TerminalScreen(80, 24).read(game.output)
                self.assertIn("SPECIMEN_INSPECTOR", visible)
                self.assertIn("Muon", visible)
                plain_output = re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", game.output)
                self.assertTrue(all(byte < 128 for byte in plain_output))
            finally:
                game.finish()
                game.close()

    def test_redirected_and_plain_step_output_have_no_ansi(self):
        with tempfile.TemporaryDirectory() as folder:
            for args in ((), ('--plain', '--step'), ('--step',)):
                with self.subTest(args=args):
                    result = self.run_game(1, '--demo', *args, input='e\n1\nq\n', cwd=folder)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertNotIn('\x1b', result.stdout)
                    self.assertIn('WIRED / OBSERVATORY', result.stdout)
                    self.assertIn('SINTETICO', result.stdout)

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_step_mode_unblocks_on_signals(self):
        with tempfile.TemporaryDirectory() as folder:
            for sig in (signal.SIGINT, signal.SIGTERM):
                with self.subTest(signal=sig):
                    game = PtyGame(self.binaries[1], folder, '--plain', '--step')
                    try:
                        game.wait_for(b'accion> ')
                        game.finish(sig)
                        self.assertEqual(game.process.returncode, 0)
                        self.assertNotIn(b'\x1b', game.output)
                        self.assertEqual(termios.tcgetattr(game.slave), game.before)
                    finally:
                        game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_plain_step_and_dumb_terminal_do_not_enter_alternate_screen(self):
        with tempfile.TemporaryDirectory() as folder:
            for args, term in ((('--plain', '--step'), 'xterm-256color'), ((), 'dumb')):
                with self.subTest(args=args, term=term):
                    game = PtyGame(self.binaries[1], folder, *args, term=term)
                    try:
                        game.wait_for(b'[Q]')
                        game.key('q\n')
                        game.process.wait(timeout=5)
                        game.read()
                        self.assertNotIn(b'\x1b', game.output)
                        self.assertEqual(termios.tcgetattr(game.slave), game.before)
                    finally:
                        game.close()

    def test_observatory_color_depth_requires_terminal_advertisement(self):
        self.assertEqual(cli.terminal_color_depth("xterm", ""), 16)
        self.assertEqual(cli.terminal_color_depth("xterm-256color", ""), 256)
        self.assertEqual(cli.terminal_color_depth("xterm", "truecolor"), 24)
        self.assertEqual(cli.terminal_color_depth("screen-direct", ""), 24)

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_topology_s03_s05_keeps_title_task_and_controls_at_80x24(self):
        references = {
            3: ("Equipo, referencias y colecciones",
                "Debilita una criatura y agregala al vector sin perder el cambio."),
            5: ("Persistencia y esquema de datos",
                "Conserva cada evento. Luego inspecciona y escribe su TTree real en ROOT."),
        }
        with tempfile.TemporaryDirectory() as folder:
            for level in (3, 5):
                with self.subTest(level=level):
                    game = PtyGame(self.binaries[level], folder, columns=80, rows=24)
                    try:
                        game.wait_for(b"[Q]")
                        visible = TerminalScreen(80, 24).read(game.output)
                        flattened = " ".join(visible.split())
                        self.assertIn("WIRED / OBSERVATORY", visible)
                        self.assertIn("EVENT_INSPECTOR", visible)
                        self.assertIn("TOPOLOGY_OBSERVATORY", visible)
                        self.assertIn("CODE_INSPECTOR", visible)
                        self.assertIn(references[level][0], visible)
                        self.assertIn(references[level][1], flattened)
                        self.assertIn("[WASD] mover", visible)
                    finally:
                        game.finish()
                        game.close()

    @unittest.skipIf(os.name == "nt", "pseudoterminal POSIX")
    def test_watch_terminal_shows_controls_on_pending_and_final_sessions(self):
        for args in (("1", "--student", "watch-ui"), ("6", "--demo")):
            with self.subTest(args=args):
                master, slave = pty.openpty()
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
                before = termios.tcgetattr(slave)
                env = dict(os.environ, TERM="xterm", COLORTERM="truecolor", LANG="C.UTF-8", LC_ALL="C.UTF-8")
                process = subprocess.Popen([sys.executable, str(ROOT / "course.py"), "watch", *args],
                                           stdin=slave, stdout=slave, stderr=slave, cwd=ROOT, env=env)
                output = bytearray()
                deadline = time.monotonic() + 30
                try:
                    while time.monotonic() < deadline and b"[q] salir" not in output:
                        ready, _, _ = select.select([master], [], [], 0.1)
                        if ready:
                            output.extend(os.read(master, 4096))
                    self.assertIn(b"[q] salir", output)
                    self.assertIn(b"\x1b[38;2;2;132;199m", output)
                    screen = TerminalScreen(80, 24)
                    visible = screen.read(b"\x1b[?1049h" + bytes(output))
                    self.assertIn("WIRED / OBSERVATORY", visible.splitlines()[0])
                    self.assertIn("[q] salir", visible.splitlines()[-1])
                    self.assertEqual(screen.scrolls, 0)
                    self.assertEqual(screen.overflow, 0)
                    if args[0] == "1":
                        start = len(output)
                        deadline = time.monotonic() + 10
                        os.write(master, b"h")
                        while time.monotonic() < deadline:
                            ready, _, _ = select.select([master], [], [], 0.1)
                            if ready:
                                output.extend(os.read(master, 4096))
                            tail = bytes(output[start:])
                            if b"Pista" in tail and b"[q] salir" in tail:
                                break
                        self.assertIn(b"Pista", output[start:])
                        screen = TerminalScreen(80, 24)
                        visible = screen.read(b"\x1b[?1049h" + bytes(output))
                        self.assertIn("Pista", visible)
                        self.assertIn("[q] salir", visible.splitlines()[-1])
                        self.assertEqual(visible.count("WIRED / OBSERVATORY"), 1)
                        self.assertEqual(screen.scrolls, 0)
                        self.assertEqual(screen.overflow, 0)
                    os.write(master, b"q")
                    process.wait(timeout=10)
                    ready, _, _ = select.select([master], [], [], 0.1)
                    if ready:
                        output.extend(os.read(master, 4096))
                    self.assertEqual(process.returncode, 0)
                    self.assertTrue(output.endswith(b"\x1b[0m"))
                    self.assertEqual(termios.tcgetattr(slave), before)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=5)
                    os.close(master)
                    os.close(slave)

    def test_watch_once_reports_next_check_and_completion(self):
        # Sin terminal, watch compila y comprueba una vez, como un check guiado.
        run = lambda *args: subprocess.run([sys.executable, str(ROOT / "course.py"), "watch", *args],
                                           cwd=ROOT, stdin=subprocess.DEVNULL, text=True,
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
        try:
            pending = run("1", "--student", "watch-probe")
            self.assertEqual(pending.returncode, 1, pending.stderr)
            self.assertIn("S01", pending.stdout)
            self.assertIn("Siguiente: El camino libre modifica el objeto original", pending.stdout)
            done = run("1", "--demo")
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertIn("Nivel completo", done.stdout)
            self.assertNotIn("\x1b", done.stdout)
        finally:
            shutil.rmtree(ROOT / ".course/watch-probe", ignore_errors=True)

    def test_package_excludes_student_data(self):
        self.assertEqual(cli.pack(), 0)
        with zipfile.ZipFile(ROOT / "dist/cpp-root-course.zip") as archive:
            names = archive.namelist()
        self.assertIn("cpp_root_course/course.py", names)
        self.assertIn("cpp_root_course/docs/syllabus.html", names)
        for path in names:
            self.assertFalse(set(Path(path).parts) & {".course", "build", "output", ".git", "dist"})


if __name__ == "__main__":
    unittest.main()
