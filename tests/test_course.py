import csv
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("course_cli", ROOT / "course.py")
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


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
            self.assertIn("Criatura incorporada al equipo", result.stdout)
            self.assertIn("EQUIPO 2/6", result.stdout)
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
    def test_terminal_restores_flags_after_interrupt(self):
        import pty
        import select
        import signal
        import termios
        import time
        with tempfile.TemporaryDirectory() as folder:
            master, slave = pty.openpty()
            before = termios.tcgetattr(slave)
            process = subprocess.Popen([str(self.binaries[1]), "--demo"], stdin=slave, stdout=slave, stderr=slave, cwd=folder)
            try:
                deadline = time.monotonic() + 10
                output = b''
                while b'[Q]' not in output and time.monotonic() < deadline:
                    ready, _, _ = select.select([master], [], [], .2)
                    if ready: output += os.read(master, 65536)
                self.assertIn(b'[Q]', output)
                during = termios.tcgetattr(slave)
                self.assertFalse(during[3] & termios.ICANON)
                process.send_signal(signal.SIGINT); process.wait(timeout=5)
                self.assertEqual(termios.tcgetattr(slave), before)
            finally:
                if process.poll() is None: process.kill(); process.wait()
                os.close(master); os.close(slave)

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
