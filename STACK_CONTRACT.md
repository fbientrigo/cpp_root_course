# Contrato del curso

- Juego nativo C++17; biblioteca estándar + termios/POSIX o consola Win32.
- Sin dependencias descargadas al compilar, sin servidor, sin CMake obligatorio.
- Python >= 3.8, solo stdlib, para comandos de compilación, comprobación y empaquetado.
- GCC o Clang. Ruta recomendada Windows: Ubuntu/WSL2. Ruta nativa: MSYS2 UCRT64 con GCC/GDB y Python en PATH.
- Depuración: `-O0 -g`, GDB, extensión Microsoft C/C++ de VS Code.
- ROOT 6 es opcional y se ejecuta aparte. Sus flags se obtienen con root-config; no se fija el estándar de ROOT a C++17 si la instalación requiere uno posterior.
- Pages: HTML/CSS/JS estáticos. Perfiles locales, sin autenticación ni sincronización remota. Importar/exportar JSON.
- Pruebas: verificaciones C++ del comportamiento, unittest de Python y node:test para progreso web. CI Ubuntu y Windows/MSYS2.
- Arte de terminal original. HP, captura y combate son reglas de juego; los datos de detector son un modelo sintético explícito.
