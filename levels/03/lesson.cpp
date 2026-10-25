#include "lesson.hpp"

namespace student {
bool capture(std::vector<course::Specimen>& team, const course::Specimen* target) {
    (void)team; (void)target;
    // TODO: validar target, vitalidad <= 50 y capacidad < 6.
    // Guarda una copia, nunca la direccion temporal del encuentro.
    // Breakpoint: compara team.size() antes/despues de push_back().
    return false;
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 3; l.title = "Equipo, referencias y colecciones";
    l.task = "Debilita una criatura y agregala al vector sin perder el cambio.";
    l.source = "levels/03/lesson.cpp";
    if (!demo) l.capture = student::capture;
    return l;
}
}
