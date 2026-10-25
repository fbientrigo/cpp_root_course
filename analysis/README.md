# Bloque de ROOT

El juego no enlaza ROOT. Esto conserva la compilación y depuración normales de C++.
Las macros usan ROOT real, nunca la animación JavaScript del syllabus.

```bash
python3 course.py batch 4 --demo --events 10000 --seed 42
python3 course.py root 4
```

Tras completar el nivel, elimina `--demo` para generar con tu criterio.
Para automatizar sin ventana: `root -l -b -q analysis/s04.C`.
En modo interactivo permanecen los histogramas y canvas para explorar.

Cada fila CSV es un **evento** con dos candidatos a muón: cuadrimomentos en GeV
(c=1), cargas en unidades de e y decisión `selected`. La TUI no interpreta el HP
como energía, masa, vida media ni eficiencia de detector. Las criaturas del catálogo
y el evento registrado por el detector son representaciones distintas.

El generador es didáctico: dos cuerpos en el centro de masa, mezcla de continuo y
pico gaussiano cerca de 3.0969 GeV; asignación sintética de carga para fondo
combinatorio. No modela sección eficaz, aceptación, resolución completa ni producción.
La correlación pT1=pT2 está impuesta. La semilla reproduce una muestra en el mismo
toolchain; las distribuciones de std::random pueden diferir entre bibliotecas de C++.
No identificar una especie ni declarar significancia a partir de una ventana de masa.

S05 convierte el CSV a un TTree con campos primitivos: no necesita diccionario de
clases. `SetBranchAddress` conecta direcciones; `GetEntry` actualiza esas variables.
La muestra guardada conserva eventos rechazados para permitir cambiar cortes después.
