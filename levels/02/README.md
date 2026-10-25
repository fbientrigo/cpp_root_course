# S02 · Clases y estado

**Antes de programar:** ¿Quién mantiene hp >= 0? ¿Un daño negativo puede curar?

## Construcción en pareja · 20 min

Archivo: `levels/02/creature.hpp`. Completa receive_damage. Luego agrega una propiedad propia conservando hp() y el constructor del adaptador.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 2
bash course.sh play 2 --student mi_alias
bash course.sh check 2 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/02/creature.hpp`. Inspecciona `this, hp_, amount`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

Inspeccionar un objeto ROOT y distinguir métodos de consulta y modificación.

```bash
bash course.sh batch 2 --demo --events 10000 --seed 42
bash course.sh root 2
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S02; importa `.course/mi_alias/progress.json` en la web.
