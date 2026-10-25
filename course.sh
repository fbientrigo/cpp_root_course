#!/usr/bin/env bash
set -euo pipefail
course_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if command -v python3 >/dev/null 2>&1; then
  exec python3 "$course_dir/course.py" "$@"
elif command -v python >/dev/null 2>&1; then
  exec python "$course_dir/course.py" "$@"
fi
echo "Falta Python 3.8+. Consulta README.md." >&2
exit 2
