#include "lesson.hpp"

namespace student {
void record(std::ostream& out, const course::Event& event, bool selected) {
    (void)out; (void)event; (void)selected;
    // TODO: escribir una fila del esquema csv_header, con '\n' al final.
    // El motor abre/cierra el archivo; ustedes deciden que informacion conservar.
    // En analysis/s05.C enlazaran esos mismos campos a las ramas de un TTree.
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 5; l.title = "Persistencia y esquema de datos";
    l.task = "Conserva cada evento. Luego inspecciona y escribe su TTree real en ROOT.";
    l.source = "levels/05/lesson.cpp";
    if (!demo) l.record = student::record;
    return l;
}
}
