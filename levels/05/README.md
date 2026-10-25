# S05 · Registro y TTree

**Antes de programar:** ¿Qué no podrás volver a analizar si solo guardas los eventos aceptados?

## Construcción en pareja · 20 min

Archivo: `levels/05/lesson.cpp`. Conserva los doce campos de csv_header y termina cada fila con un salto de línea. La exportación se bloquea mientras falla el contrato.

El motor ya entrega entrada, dibujo y encuentros. La referencia de otras operaciones
mantiene este checkpoint ejecutable. Pueden cambiar el diseño interno de su operación.

```bash
bash course.sh demo 5
bash course.sh play 5 --student mi_alias
bash course.sh check 5 --student mi_alias
```

## Breakpoint

Detente dentro de la operación en `levels/05/lesson.cpp`. Inspecciona `event, selected, out`.
Usa F10 para observar el cambio y F11 para entrar en una llamada. Primero predice;
después ejecuta. F5 pide un nivel y compila en modo debug.

## ROOT · 20 min

En analysis/s05.C, observar Branch/Fill/Write y SetBranchAddress/GetEntry sobre un TTree real.

```bash
bash course.sh batch 5 --demo --events 10000 --seed 42
bash course.sh root 5
```

Cuando tu operación pase las comprobaciones, repite sin `--demo` para trabajar con
su implementación. La semilla y el toolchain deben coincidir al comparar resultados.

## Evidencia de aprendizaje

El alumno puede explicar qué recibe la operación, qué modifica, qué conserva y
por qué pasan los casos límite de `check`. Una comprobación aprobada habilita el
logro S05; importa `.course/mi_alias/progress.json` en la web.
