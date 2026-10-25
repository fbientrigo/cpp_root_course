#include "lesson.hpp"
#include "creature.hpp"

namespace student {
int apply_damage(int current_hp, int amount) {
    Creature creature{current_hp};
    creature.receive_damage(amount); // Breakpoint: this, hp_, amount.
    return creature.hp();
}
}
namespace course {
Lesson make_lesson(bool demo) {
    auto l = reference_lesson();
    l.number = 2; l.title = "Clases y estado privado";
    l.task = "Implementa Creature::receive_damage(). Vitalidad es una regla ludica.";
    l.source = "levels/02/creature.hpp";
    if (!demo) l.damage = student::apply_damage;
    return l;
}
}
