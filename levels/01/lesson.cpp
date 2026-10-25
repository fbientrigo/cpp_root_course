#include "lesson.hpp"

namespace student {
// S01: entrada ya normalizada; implementa la decision y la mutacion.
// Breakpoint aqui. Observa player, input y next antes/despues de avanzar.
bool move(course::Position& player, course::Input input, const course::World& world) {
    (void)player; (void)input; (void)world;
    // TODO: construir la posicion candidata, consultar world.walkable(next),
    // modificar player solamente si se puede avanzar y devolver si se movio.
    return false;
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 1; l.title = "Movimiento y contratos";
    l.task = "Conecta Input -> Position. Respeta paredes y limites.";
    l.source = "levels/01/lesson.cpp";
    if (!demo) l.move = student::move;
    return l;
}
}
