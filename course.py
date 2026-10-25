#!/usr/bin/env python3
"""Comandos del curso. Python 3.8+, biblioteca estandar, sin instalacion pip."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parent


def compiler():
    candidates = [os.environ["CXX"]] if os.environ.get("CXX") else ["g++", "clang++"]
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError("Falta GCC/Clang en PATH. Ejecuta doctor y consulta README.md; CXX puede indicar el ejecutable.")


class CompileError(RuntimeError):
    def __init__(self, message, output):
        super().__init__(message)
        self.output = output


def build(level, quiet=False):
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
        # Compilar a un temporal y reemplazar de una vez: watch y F5 pueden
        # compilar el mismo nivel a la vez sin dejar un binario a medias.
        partial = binary.with_name("%s.%d.tmp" % (binary.name, os.getpid()))
        cmd = [cxx] + flags + [str(p) for p in sources] + ["-o", str(partial)]
        if not quiet:
            print("[BUILD] S%02d / debug (-O0 -g)" % level, flush=True)
        result = subprocess.run(cmd, cwd=ROOT, capture_output=quiet, text=True)
        if result.returncode:
            partial.unlink(missing_ok=True)
            raise CompileError("El compilador encontro un problema. El primer mensaje indica archivo y linea.",
                               result.stderr if quiet else "")
        os.replace(str(partial), str(binary))
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


def level_info(level):
    """Titulo, archivo, tarea y pistas: el README del nivel es la unica fuente."""
    text = (ROOT / ("levels/%02d/README.md" % level)).read_text(encoding="utf-8")
    def find(pattern, default=""):
        match = re.search(pattern, text)
        return match.group(1).strip() if match else default
    return {
        "title": text.splitlines()[0].lstrip("# ").strip(),
        "question": find(r"\*\*Antes de programar:\*\*\s*(.+)"),
        "source": find(r"Archivo: `([^`]+)`", "levels/%02d/lesson.cpp" % level),
        "task": find(r"Archivo: `[^`]+`\.\s*(.+)"),
        "inspect": find(r"Inspecciona `([^`]+)`"),
    }


class Keys:
    """Una tecla sin Enter, con espera limitada; restaura la terminal al salir."""
    def __enter__(self):
        if os.name != "nt":
            import termios
            import tty
            self.fd = sys.stdin.fileno()
            self.saved = termios.tcgetattr(self.fd)
            tty.setcbreak(self.fd)
        return self

    def __exit__(self, *error):
        self.pause()

    def pause(self):
        if os.name != "nt":
            import termios
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.saved)

    def resume(self):
        if os.name != "nt":
            import tty
            tty.setcbreak(self.fd)

    def read(self, timeout):
        if os.name == "nt":
            import msvcrt
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                if msvcrt.kbhit():
                    return msvcrt.getwch()
                time.sleep(0.05)
            return None
        import select
        ready, _, _ = select.select([sys.stdin], [], [], timeout)
        return os.read(self.fd, 1).decode(errors="ignore") if ready else None


def attempt(level, student, demo):
    """Compila y comprueba un nivel. Devuelve (estado, lineas) para mostrar."""
    try:
        binary = build(level, quiet=True)
    except CompileError as error:
        # Rutas relativas: el alumno busca levels/0N/..., no la ruta absoluta.
        lines = [l.replace(str(ROOT) + os.sep, "") for l in error.output.splitlines() if l.strip()]
        first = next((l for l in lines if "error" in l), lines[0] if lines else "")
        return "compile", [first] + [l for l in lines if l != first][:14]
    command = [str(binary), "--check", "--student", student] + (["--demo"] if demo else [])
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        return "crash", ["Las pruebas no terminaron en 10 s: revisa si hay un bucle infinito."]
    checks = [l for l in result.stdout.splitlines() if l.startswith(("[OK]", "[TODO]"))]
    if result.returncode < 0 or result.returncode > 2 or (result.returncode and not checks):
        # Negativo en POSIX (senal); 0xC0000005 en Windows (acceso invalido).
        return "crash", ["El programa se cerro antes de terminar las pruebas (codigo %d)." % result.returncode,
                         "Suele ser un puntero nulo o un indice fuera de rango.",
                         "Pon un breakpoint en tu funcion y ejecuta con F5."] + (
                            ["Pruebas completadas antes del fallo:"] + ["  " + c for c in checks] if checks else [])
    return ("passed" if result.returncode == 0 else "pending"), checks


def watch(first_level, student, demo, plain):
    interactive = sys.stdin.isatty() and sys.stdout.isatty()
    color = interactive and not plain and os.environ.get("TERM") != "dumb"
    utf8 = (sys.stdout.encoding or "").lower().replace("-", "") == "utf8"
    if os.name == "nt" and color:
        os.system("")  # Activa las secuencias VT en la consola de Windows.
    ok, todo = ("✓", "✗") if utf8 else ("OK", "--")

    def paint(code, text):
        return "\033[%sm%s\033[0m" % (code, text) if color else text

    profile = ROOT / ".course" / ("demo" if demo else student) / "earned.txt"
    passed = set()

    def done(level):
        if level in passed:
            return True
        return profile.exists() and ("check0%d" % level) in profile.read_text().split()

    level = first_level or next((i for i in range(1, 7) if not done(i)), 6)
    show_hint = False

    def screen(state, lines):
        info = level_info(level)
        out = []
        bar = "".join(paint("92", ok if utf8 else "#") if done(i) else
                      paint("96", "○" if utf8 else "o") if i == level else
                      paint("90", "·" if utf8 else ".") for i in range(1, 7))
        finished = sum(done(i) for i in range(1, 7))
        out.append(paint("1", "ROOT QUEST") + "  " + bar + "  %d/6" % finished
                   + paint("90", "   alias: " + ("demo" if demo else student)))
        out.append("")
        out.append(paint("1", info["title"]))
        out.append(paint("90", "Archivo  ") + paint("96", info["source"]))
        out.append(paint("90", "Tarea    ") + info["task"])
        out.append("")
        if state == "compile":
            out.append(paint("91", todo + " No compila.") + " Lee solo el primer error: indica archivo y linea.")
            out.append("")
            out += ["  " + paint("91" if i == 0 else "90", l) for i, l in enumerate(lines)]
        elif state == "crash":
            out.append(paint("91", todo + " Se cayo al probar."))
            out += ["  " + l for l in lines]
        else:
            good = sum(l.startswith("[OK]") for l in lines)
            out.append(paint("92" if state == "passed" else "93", "PRUEBAS %d/%d" % (good, len(lines))))
            for l in lines:
                mark = paint("92", ok) if l.startswith("[OK]") else paint("93", todo)
                out.append("  %s %s" % (mark, l.split("]", 1)[1].strip()))
            out.append("")
            if state == "passed":
                out.append(paint("92", "Nivel completo.") + " Antes de seguir, responde en voz alta:")
                out.append("  " + info["question"])
            else:
                pending = next(l for l in lines if l.startswith("[TODO]")).split("]", 1)[1].strip()
                out.append(paint("1", "Siguiente: ") + pending)
                out.append(paint("90", "Guarda el archivo y se vuelve a comprobar solo." if interactive
                                 else "Corrige y ejecuta: python3 course.py watch"))
        if show_hint and state != "passed":
            out.append("")
            out.append(paint("96", "PISTA"))
            out.append("  Antes de escribir: " + info["question"])
            if info["inspect"]:
                out.append("  Breakpoint en %s; inspecciona %s." % (info["source"], info["inspect"]))
            out.append("  Compara con la referencia jugando: python3 course.py demo %d" % level)
        if not interactive:
            print("\n".join(out), flush=True)
            return
        out.append("")
        keys = ["[h] pista", "[p] jugar", "[1-6] nivel", "[q] salir"]
        if state == "passed" and level < 6:
            keys.insert(0, "[n] siguiente")
        out.append(paint("96", "  ".join(keys)))
        if color:
            sys.stdout.write("\033[H\033[2J")
        print("\n".join(out), flush=True)

    def sources():
        folder = ROOT / ("levels/%02d" % level)
        return {p: p.stat().st_mtime for p in sorted(folder.glob("*.[ch]pp"))}

    state, lines = attempt(level, student, demo)
    if state == "passed":
        passed.add(level)
    if not interactive:
        screen(state, lines)
        return 0 if state == "passed" else 1
    with Keys() as keys:
        seen = sources()
        screen(state, lines)
        while True:
            key = keys.read(0.4)
            current = sources()
            rerun = current != seen
            if key in ("q", "Q", "\x03"):
                break
            if key in ("h", "H"):
                show_hint = not show_hint
                screen(state, lines)
            elif key in ("n", "N") and state == "passed" and level < 6:
                level, show_hint, rerun = level + 1, False, True
            elif key and key in "123456":
                level, show_hint, rerun = int(key), False, True
            elif key in ("r", "R"):
                rerun = True
            elif key in ("p", "P") and state != "compile":
                keys.pause()
                command = [str(build(level, quiet=True)), "--student", student] + (["--demo"] if demo else [])
                subprocess.call(command, cwd=ROOT)
                keys.resume()
                rerun = True
            if rerun:
                seen = sources()
                print(paint("90", "compilando y probando S%02d..." % level), flush=True)
                state, lines = attempt(level, student, demo)
                if state == "passed":
                    passed.add(level)
                screen(state, lines)
    return 0


def main():
    parser = argparse.ArgumentParser(description="ROOT Quest: compilar, jugar, verificar, analizar.")
    parser.add_argument("command", choices=["watch", "doctor", "setup-vscode", "build", "play", "demo", "check", "batch", "progress", "pack", "root"])
    parser.add_argument("level", nargs="?", choices=[str(i) for i in range(1, 7)] + ["all"])
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
    if args.command == "watch":
        if args.level == "all":
            parser.error("watch recorre los niveles en orden; indica 1-6 o ninguno")
        return watch(int(args.level) if args.level else None, args.student, args.demo, args.plain)
    args.level = args.level or "1"
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
