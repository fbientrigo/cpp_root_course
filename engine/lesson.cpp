#include "lesson.hpp"
#include <iostream>
#include <sstream>

namespace course {
Lesson reference_lesson() {
    Lesson l;
    l.move = [](Position& p, Input i, const World& w) {
        Position next{p.x + i.dx, p.y + i.dy};
        if ((i.dx == 0 && i.dy == 0) || !w.walkable(next)) return false;
        p = next;
        return true;
    };
    l.damage = [](int hp, int amount) { return std::max(0, hp - std::max(0, amount)); };
    l.capture = [](std::vector<Specimen>& team, const Specimen* wild) {
        if (!wild || wild->hp > 50 || team.size() >= 6) return false;
        team.push_back(*wild); // Copia propia; no conserva el puntero prestado.
        return true;
    };
    l.select = [](const Event& e, double cut) {
        return e.q1 * e.q2 < 0 && e.a.pt() >= cut && e.b.pt() >= cut;
    };
    l.record = write_row;
    l.in_window = [](double mass, double low, double high) { return low <= mass && mass <= high; };
    return l;
}
int check_lesson(const Lesson& l, std::ostream& out) {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        // Flush: si el codigo del alumno se cae, quedan visibles las ya pasadas.
        out << (ok ? "[OK]   " : "[TODO] ") << message << '\n' << std::flush;
        if (!ok) ++failures;
    };
    switch (l.number) {
    case 1: {
        World w; Position p{1, 1};
        check(!l.move(p, {-1, 0}, w) && p.x == 1 && p.y == 1, "Una pared conserva la posicion");
        check(l.move(p, {1, 0}, w) && p.x == 2 && p.y == 1, "El camino libre modifica el objeto original");
        Position edge{0, 0};
        check(!l.move(edge, {-1, 0}, w), "El borde no accede fuera del mapa");
        break;
    }
    case 2:
        check(l.damage(80, 25) == 55, "El metodo cambia el estado de la criatura");
        check(l.damage(10, 25) == 0, "La vitalidad no baja de cero");
        check(l.damage(80, -10) == 80, "Un dano negativo no cura");
        break;
    case 3: {
        std::vector<Specimen> team; Specimen wild{1, 50};
        check(!l.capture(team, nullptr), "Un objetivo ausente no se desreferencia");
        const bool caught = l.capture(team, &wild);
        check(caught && team.size() == 1 && team[0].species == 1, "La captura agrega una copia al equipo original");
        wild.hp = 100;
        check(!l.capture(team, &wild), "Una criatura con vitalidad alta no se captura");
        wild.hp = 10; team.assign(6, wild);
        check(!l.capture(team, &wild) && team.size() == 6, "El equipo tiene capacidad de seis");
        break;
    }
    case 4: {
        Event e; e.a.px = 2; e.b.py = 2;
        check(l.select(e, 2), "El umbral incluye la igualdad");
        e.q2 = 1;
        check(!l.select(e, 2), "Se rechazan cargas del mismo signo");
        e.q2 = -1; e.a.px = 0.5;
        check(!l.select(e, 2), "El corte se aplica a ambos candidatos");
        break;
    }
    case 5: {
        Event e; e.id = 7; e.a = {1, 2, 3, 4}; e.b = {-1, -2, -3, 4};
        std::ostringstream actual, expected;
        l.record(actual, e, true); write_row(expected, e, true);
        const auto text = actual.str();
        check(text == expected.str(), "Se conserva el esquema y cada valor del evento");
        check(std::count(text.begin(), text.end(), ',') == 11, "Doce columnas por evento");
        break;
    }
    case 6:
        check(l.in_window(3.1, 3.0, 3.2), "La ventana conserva el candidato interior");
        check(!l.in_window(2.8, 3.0, 3.2), "La ventana rechaza el exterior");
        check(l.in_window(3.0, 3.0, 3.2) && l.in_window(3.2, 3.0, 3.2), "Los extremos son inclusivos");
        check(!l.in_window(3.1, 3.2, 3.0), "Una ventana invertida no acepta candidatos");
        break;
    }
    return failures == 0 ? 0 : 1;
}
} // namespace course
