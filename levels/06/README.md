# S06 · Análisis propio

**Antes de programar:** ¿Cambiar una ventana permite concluir que identificaste una especie? ¿Qué sesgo introduce?

## Construcción en pareja · 20 min

Archivo: `levels/06/lesson.cpp`. Implementa una ventana inclusiva, luego crea y prueba una variante del criterio con tu instructor.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 6
bash course.sh play 6 --student mi_alias
bash course.sh check 6 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/06/lesson.cpp`. Inspecciona `mass, low, high`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

Comparar con la misma semilla, exportar report.pdf y explicar lo observado y sus límites.

```bash
bash course.sh batch 6 --demo --events 10000 --seed 42
bash course.sh root 6
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S06; importa `.course/mi_alias/progress.json` en la web.
