$ErrorActionPreference = "Stop"
$courseScript = Join-Path $PSScriptRoot "course.py"
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $courseScript @args
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python $courseScript @args
} else {
    throw "Falta Python 3.8+. Consulta README.md."
}
exit $LASTEXITCODE
