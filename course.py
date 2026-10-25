#!/usr/bin/env python3
"""Comandos del curso. Python 3.8+, biblioteca estandar, sin instalacion pip."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent


def compiler():
    candidates = [os.environ["CXX"]] if os.environ.get("CXX") else ["g++", "clang++"]
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError("Falta GCC/Clang en PATH. Ejecuta doctor y consulta README.md; CXX puede indicar el ejecutable.")


def build(level):
    cxx = compiler()
    folder = ROOT / "build" / ("level%02d" % level)
    folder.mkdir(parents=True, exist_ok=True)
    binary = folder / ("game.exe" if os.name == "nt" else "game")
    sources = [ROOT / "engine/main.cpp", ROOT / "engine/lesson.cpp", ROOT / ("levels/%02d/lesson.cpp" % level)]
    dependencies = list((ROOT / "engine").glob("*.hpp")) + list((ROOT / ("levels/%02d" % level)).glob("*.hpp")) + sources + [Path(__file__)]
    flags = ["-std=c++17", "-g", "-O0", "-Wall", "-Wextra", "-Wpedantic", "-I" + str(ROOT / "engine")]
    signature = json.dumps([cxx, flags])
    signature_path = folder / "compiler.json"
    stale = (not binary.exists() or not signature_path.exists() or signature_path.read_text() != signature or
             any(p.stat().st_mtime > binary.stat().st_mtime for p in dependencies))
    if stale:
        cmd = [cxx] + flags + [str(p) for p in sources] + ["-o", str(binary)]
        print("[BUILD] S%02d / debug (-O0 -g)" % level, flush=True)
        result = subprocess.run(cmd, cwd=ROOT)
        if result.returncode:
            raise RuntimeError("El compilador encontro un problema. El primer mensaje indica archivo y linea.")
        signature_path.write_text(signature)
    # Index all levels, not just the most recently built one. This keeps
    # clangd navigation and diagnostics valid when students change sessions.
    indexed_sources = [ROOT / "engine/main.cpp", ROOT / "engine/lesson.cpp"] + [
        ROOT / ("levels/%02d/lesson.cpp" % i) for i in range(1, 7)
    ]
    commands = [{"directory": str(ROOT), "file": str(p), "arguments": [cxx] + flags + ["-c", str(p)]} for p in indexed_sources]
    (ROOT / "build/compile_commands.json").write_text(json.dumps(commands, indent=2))
    return binary


def setup_vscode():
    """Install the workspace's recommended VS Code extensions with the CLI."""
    code = shutil.which("code") or shutil.which("code-insiders")
    if not code:
        raise RuntimeError("No se encontro 'code' en PATH. En VS Code abre la paleta (Ctrl+Shift+P), ejecuta 'Shell Command: Install code command in PATH' (si esta disponible), o instala las recomendaciones en Extensiones.")
    extensions = json.loads((ROOT / ".vscode/extensions.json").read_text())["recommendations"]
    for extension in extensions:
        print("[VSCODE] " + extension, flush=True)
        if subprocess.call([code, "--install-extension", extension]) != 0:
            raise RuntimeError("No se pudo instalar " + extension + ". Revisa la conexion y prueba desde Extensiones en VS Code.")
    print("[OK] Extensiones solicitadas. clangd requiere su servidor de lenguaje y build/compile_commands.json; ejecuta 'python course.py build 1'.")
    return 0


def doctor():
    print("Python:", sys.version.split()[0])
    for name in ["g++", "clang++", "clangd", "gdb", "root", "root-config", "git"]:
        print("%-12s %s" % (name + ":", shutil.which(name) or "no encontrado"))
    try:
        print("Compilador elegido:", compiler())
    except RuntimeError as error:
        print(error)
        return 1
    print("Juego: no necesita ROOT. VS Code: clangd para editar; GDB y C/C++ para F5. Sonido POSIX: campana del terminal.")
    return 0


def pack():
    # Solo archivos del curso; nunca incluir perfiles, resultados o binarios locales.
    destination = ROOT / "dist/cpp-root-course.zip"
    destination.parent.mkdir(exist_ok=True)
    excluded = {".git", ".course", "build", "dist", "output", "__pycache__", "node_modules"}
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for p in sorted(ROOT.rglob("*")):
            relative = p.relative_to(ROOT)
            if p.is_file() and not any(part in excluded for part in relative.parts) and p.suffix != ".pyc":
                archive.write(p, Path("cpp_root_course") / relative)
    print(destination)
    return 0


def main():
    parser = argparse.ArgumentParser(description="ROOT Quest: compilar, jugar, verificar, analizar.")
    parser.add_argument("command", choices=["doctor", "setup-vscode", "build", "play", "demo", "check", "batch", "progress", "pack", "root"])
    parser.add_argument("level", nargs="?", choices=[str(i) for i in range(1, 7)] + ["all"], default="1")
    parser.add_argument("--student", default="alumno", help="alias local ASCII, 1-32 caracteres")
    parser.add_argument("--demo", action="store_true", help="referencia funcional; no acredita progreso")
    parser.add_argument("--step", action="store_true", help="una accion + Enter; util en el depurador")
    parser.add_argument("--plain", action="store_true", help="sin secuencias ANSI")
    parser.add_argument("--sound", action="store_true", help="activar tonos Windows/campana del terminal")
    parser.add_argument("--debug-active", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--events", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.command == "doctor":
        return doctor()
    if args.command == "setup-vscode":
        return setup_vscode()
    if args.command == "pack":
        return pack()
    if args.level == "all" and args.command not in ("build", "check"):
        parser.error("all solo se aplica a build y check")
    if not 1 <= args.events <= 1000000 or not 0 <= args.seed <= 4294967295:
        parser.error("events debe estar en 1..1000000 y seed en 0..4294967295")
    if not args.student or len(args.student) > 32 or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in args.student):
        parser.error("student: usa 1-32 letras ASCII, numeros, - o _")
    if args.command == "progress":
        path = ROOT / ".course" / args.student / "progress.json"
        if not path.exists():
            raise RuntimeError("Aun no hay progreso. Juega o verifica un nivel con --student " + args.student)
        print(path)
        print(path.read_text())
        return 0
    if args.command == "root":
        root = shutil.which("root")
        if not root:
            raise RuntimeError("ROOT es opcional. Activa tu instalacion de ROOT 6 y vuelve a ejecutar este comando.")
        path = ROOT / ("output/events-s0%s.csv" % args.level)
        if not path.exists():
            raise RuntimeError("Primero genera datos: python3 course.py batch " + args.level + " --demo")
        return subprocess.call([root, "-l", "analysis/s0%s.C" % args.level], cwd=ROOT)
    status = 0
    for level in range(1, 7) if args.level == "all" else [int(args.level)]:
        binary = build(level)
        if args.command == "build":
            if args.debug_active:
                active = ROOT / "build/debug" / binary.name
                active.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(binary, active)
            print("[OK]", binary.relative_to(ROOT))
            continue
        command = [str(binary), "--student", args.student]
        if args.demo or args.command == "demo":
            command.append("--demo")
        for key in ("step", "plain", "sound"):
            if getattr(args, key):
                command.append("--" + key)
        if args.command == "check":
            command.append("--check")
        elif args.command == "batch":
            command += ["--batch", str(args.events), "--seed", str(args.seed)]
        status = max(status, subprocess.call(command, cwd=ROOT))
    return status


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError) as error:
        print("[ERROR]", error, file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        sys.exit(130)
