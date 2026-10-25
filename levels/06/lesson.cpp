#include "lesson.hpp"

namespace student {
bool in_window(double mass, double low, double high) {
    (void)mass; (void)low; (void)high;
    // TODO: criterio propio de seleccion, primero con ventana inclusiva.
    // Despues: compara eficiencia/pureza con el instructor en datos sinteticos.
    // Una masa compatible no demuestra por si sola la identidad de una particula.
    return false;
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 6; l.title = "Analisis autonomo";
    l.task = "Predice, selecciona y explica el exceso del conjunto sintetico.";
    l.source = "levels/06/lesson.cpp";
    if (!demo) l.in_window = student::in_window;
    return l;
}
}
