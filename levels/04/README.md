# S04 · Selección de eventos

**Antes de programar:** ¿Qué perderás al subir el corte? ¿La igualdad se acepta?

## Construcción en pareja · 20 min

Archivo: `levels/04/lesson.cpp`. Usa ambos pT y el producto de cargas. El evento se consulta por referencia constante.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 4
bash course.sh play 4 --student mi_alias
bash course.sh check 4 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/04/lesson.cpp`. Inspecciona `event.q1, event.q2, event.a.pt(), min_pt`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

Sumar cuadrimomentos en ROOT y comparar el espectro completo con la selección.

```bash
bash course.sh batch 4 --demo --events 10000 --seed 42
bash course.sh root 4
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S04; importa `.course/mi_alias/progress.json` en la web.
