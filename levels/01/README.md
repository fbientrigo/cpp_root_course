# S01 · Movimiento y contratos

**Antes de programar:** ¿Qué cambia si Position se pasa por valor? ¿La pared debe modificar alguna coordenada?

## Construcción en pareja · 20 min

Archivo: `levels/01/lesson.cpp`. Implementa una posición candidata y consulta World::walkable antes de modificar player.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 1
bash course.sh play 1 --student mi_alias
bash course.sh check 1 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/01/lesson.cpp`. Inspecciona `player, input, next`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

Construir TH1D, Fill y Draw con los datos de pT de la misma clase Generator.

```bash
bash course.sh batch 1 --demo --events 10000 --seed 42
bash course.sh root 1
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S01; importa `.course/mi_alias/progress.json` en la web.
