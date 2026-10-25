#pragma once
#include "domain.hpp"
#include <functional>

namespace course {
// Contrato pequeno entre el motor (entrada/dibujo) y el codigo del nivel.
// Las seis lecciones usan el mismo escenario; solo cambia la operacion estudiada.
struct Lesson {
    int number = 1;
    const char* title = "";
    const char* task = "";
    const char* source = "";
    std::function<bool(Position&, Input, const World&)> move;
    std::function<int(int, int)> damage;
    std::function<bool(std::vector<Specimen>&, const Specimen*)> capture;
    std::function<bool(const Event&, double)> select;
    std::function<void(std::ostream&, const Event&, bool)> record;
    std::function<bool(double, double, double)> in_window;
};
Lesson reference_lesson();
Lesson make_lesson(bool demo);
int check_lesson(const Lesson& lesson);
} // namespace course
