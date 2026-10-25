#include "lesson.hpp"

namespace student {
bool select(const course::Event& event, double min_pt) {
    (void)event; (void)min_pt;
    // TODO: cargas opuestas y ambos pT >= min_pt.
    // event.a.pt(), event.b.pt(). No modificar el evento original.
    return false;
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 4; l.title = "Seleccion y masa invariante";
    l.task = "Selecciona pares de cargas opuestas. Predice como cambia el espectro.";
    l.source = "levels/04/lesson.cpp";
    if (!demo) l.select = student::select;
    return l;
}
}
