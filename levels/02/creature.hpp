#pragma once

namespace student {
// Este modelo les pertenece. Pueden agregar nombre, especie y metodos.
// El pequeno adaptador de lesson.cpp solo necesita hp() y receive_damage().
class Creature {
    int hp_;
public:
    explicit Creature(int hp) : hp_(hp) {}
    int hp() const { return hp_; }
    void receive_damage(int amount) {
        (void)amount;
        // TODO: cambiar hp_ manteniendo 0 <= hp_ y sin curar con dano negativo.
    }
};
}
