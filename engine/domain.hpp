#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

namespace course {
struct Position { int x = 2; int y = 2; };
struct Input { int dx = 0; int dy = 0; };
struct World {
    std::vector<std::string> rows = {
        "#######################", "#.....#...............#",
        "#.....#....:::........#", "#..........:::........#",
        "#..:::.........#......#", "#..:::.........#......#",
        "#..............#..:::##", "#.....................#",
        "#######################"};
    bool walkable(Position p) const {
        return p.y >= 0 && p.y < static_cast<int>(rows.size()) && p.x >= 0 &&
            p.x < static_cast<int>(rows[static_cast<std::size_t>(p.y)].size()) &&
            rows[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] != '#';
    }
};
enum class Family { quark, lepton, boson };
struct Species { const char* name; const char* symbol; Family family; int charge3; };
// charge3 = 3*q/e: representa exactamente las cargas fraccionarias.
inline const std::vector<Species>& catalogue() {
    static const std::vector<Species> entries = {
        {"Electron", "e-", Family::lepton, -3}, {"Muon", "mu-", Family::lepton, -3},
        {"Tau", "tau-", Family::lepton, -3}, {"Neutrino electronico", "nu_e", Family::lepton, 0},
        {"Neutrino muonico", "nu_mu", Family::lepton, 0}, {"Neutrino tauonico", "nu_tau", Family::lepton, 0},
        {"Up", "u", Family::quark, 2}, {"Down", "d", Family::quark, -1},
        {"Charm", "c", Family::quark, 2}, {"Strange", "s", Family::quark, -1},
        {"Top", "t", Family::quark, 2}, {"Bottom", "b", Family::quark, -1},
        {"Foton", "gamma", Family::boson, 0}, {"Gluon", "g", Family::boson, 0},
        {"W+", "W+", Family::boson, 3}, {"Z", "Z0", Family::boson, 0},
        {"Higgs", "H", Family::boson, 0}};
    return entries;
}
inline const char* family_name(Family f) {
    return f == Family::quark ? "QUARK" : f == Family::lepton ? "LEPTON" : "BOSON";
}
struct Specimen { std::size_t species = 0; int hp = 100; };
struct FourVector {
    double px = 0, py = 0, pz = 0, energy = 0;
    double pt() const { return std::hypot(px, py); }
};
struct Event {
    int id = 0;
    FourVector a, b;
    int q1 = 1, q2 = -1;
    double mass() const {
        const double e = a.energy + b.energy;
        const double x = a.px + b.px, y = a.py + b.py, z = a.pz + b.pz;
        return std::sqrt(std::max(0.0, e*e - x*x - y*y - z*z));
    }
};
inline constexpr const char* csv_header = "event_id,px1,py1,pz1,E1,px2,py2,pz2,E2,q1,q2,selected\n";
// Una fila representa un evento de detector, con dos candidatos a muon.
inline void write_row(std::ostream& out, const Event& e, bool selected) {
    out << e.id << ',' << e.a.px << ',' << e.a.py << ',' << e.a.pz << ',' << e.a.energy << ','
        << e.b.px << ',' << e.b.py << ',' << e.b.pz << ',' << e.b.energy << ','
        << e.q1 << ',' << e.q2 << ',' << (selected ? 1 : 0) << '\n';
}
} // namespace course
