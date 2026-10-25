"""VS Code setup and clangd compilation database regression tests."""
import importlib.util
import json
from pathlib import Path
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("course_vscode_cli", ROOT / "course.py")
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class VSCodeSetupTest(unittest.TestCase):
    def test_recommendations_and_no_duplicate_semantic_engine(self):
        extensions = json.loads((ROOT / ".vscode/extensions.json").read_text())["recommendations"]
        settings = json.loads((ROOT / ".vscode/settings.json").read_text())
        self.assertIn("llvm-vs-code-extensions.vscode-clangd", extensions)
        self.assertIn("usernamehw.errorlens", extensions)
        self.assertIn("ms-vscode.cpptools", extensions)  # GDB debugger
        self.assertEqual(settings["C_Cpp.intelliSenseEngine"], "disabled")
        self.assertIn("CompilationDatabase: build", (ROOT / ".clangd").read_text())

    def test_setup_installs_exactly_workspace_recommendations(self):
        expected = json.loads((ROOT / ".vscode/extensions.json").read_text())["recommendations"]
        with mock.patch.object(cli.shutil, "which", return_value="/fake/code"), \
             mock.patch.object(cli.subprocess, "call", return_value=0) as install:
            self.assertEqual(cli.setup_vscode(), 0)
        self.assertEqual(install.call_args_list,
                         [mock.call(["/fake/code", "--install-extension", item]) for item in expected])

    def test_setup_without_code_cli_has_actionable_error(self):
        with mock.patch.object(cli.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "No se encontro 'code'"):
                cli.setup_vscode()

    def test_compile_commands_index_all_six_levels(self):
        cli.build(1)
        database = json.loads((ROOT / "build/compile_commands.json").read_text())
        names = {Path(entry["file"]).relative_to(ROOT).as_posix() for entry in database}
        self.assertEqual(names, {"engine/main.cpp", "engine/lesson.cpp"} |
                         {"levels/%02d/lesson.cpp" % level for level in range(1, 7)})
        self.assertTrue(all("-std=c++17" in entry["arguments"] for entry in database))


if __name__ == "__main__":
    unittest.main()
