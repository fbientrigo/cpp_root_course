# S03 · Equipo y memoria

**Antes de programar:** ¿Qué pasa si target es nullptr? ¿Qué debe sobrevivir cuando cambia el encuentro?

## Construcción en pareja · 20 min

Archivo: `levels/03/lesson.cpp`. Valida el objetivo prestado, hp <= 50 y capacidad < 6. Guarda por valor dentro del equipo recibido por referencia.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 3
bash course.sh play 3 --student mi_alias
bash course.sh check 3 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/03/lesson.cpp`. Inspecciona `team.size(), target, *target`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

Recorrer una colección, llenar histogramas y explicar punto/flecha/propiedad.

```bash
bash course.sh batch 3 --demo --events 10000 --seed 42
bash course.sh root 3
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S03; importa `.course/mi_alias/progress.json` en la web.
