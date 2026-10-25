#pragma once
#include "lesson.hpp"
#include "terminal.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <stdexcept>

namespace course {
inline bool valid_student(const std::string& name) {
    return !name.empty() && name.size() <= 32 && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
class Progress {
    std::string student_;
    std::filesystem::path dir_;
    std::set<std::string> earned_;
    static bool known(const std::string& id) {
        return id == "first_move" || id == "first_capture" || id == "first_selection" ||
            id == "first_export" || (id.size() == 7 && id.substr(0, 6) == "check0" && id[6] >= '1' && id[6] <= '6');
    }
public:
    Progress(const std::string& student, bool demo) : student_(student),
        dir_(std::filesystem::path(".course") / (demo ? "demo" : student)) {
        if (!valid_student(student)) throw std::runtime_error("Perfil: usa 1-32 letras ASCII, numeros, - o _.");
        std::filesystem::create_directories(dir_);
        std::ifstream in(dir_ / "earned.txt");
        std::string id;
        while (in >> id) if (known(id)) earned_.insert(id);
    }
    bool award(const std::string& id) {
        if (!known(id) || !earned_.insert(id).second) return false;
        std::ofstream out(dir_ / "earned.txt", std::ios::app);
        if (!(out << id << '\n')) throw std::runtime_error("No se pudo guardar el logro.");
        return true;
    }
    std::size_t count() const { return earned_.size(); }
    std::filesystem::path export_json(bool demo) const {
        const auto path = dir_ / "progress.json";
        std::ofstream out(path);
        out << "{\"schema\":1,\"course\":\"cpp-root-course\",\"student\":\"" << student_
            << "\",\"demo\":" << (demo ? "true" : "false") << ",\"achievements\":[";
        bool comma = false;
        for (const auto& id : earned_) { if (comma) out << ','; out << '"' << id << '"'; comma = true; }
        out << "],\"sessions\":{";
        for (int i = 1; i <= 6; ++i) {
            if (i > 1) out << ',';
            out << '"' << i << "\":{\"pair\":" << (earned_.count("check0" + std::to_string(i)) ? "true" : "false") << '}';
        }
        if (!(out << "}}\n")) throw std::runtime_error("No se pudo exportar el progreso.");
        return path;
    }
};
class Generator {
    std::mt19937 rng_;
public:
    explicit Generator(unsigned seed) : rng_(seed) {}
    Event next(int id) {
        // Modelo didactico: dos muones en el CM; mezcla de continuo y pico.
        // No es una simulacion de produccion, detector ni significancia.
        std::uniform_real_distribution<double> uniform(0, 1);
        std::normal_distribution<double> peak(3.0969, 0.055);
        const double mass = uniform(rng_) < 0.20 ? std::max(0.22, peak(rng_)) : 0.4 + 5.6 * uniform(rng_);
        const double energy = mass / 2, muon_mass = 0.1056584;
        const double p = std::sqrt(energy*energy - muon_mass*muon_mass);
        const double cos_theta = 2 * uniform(rng_) - 1;
        const double sin_theta = std::sqrt(1 - cos_theta*cos_theta);
        const double phi = 6.283185307179586 * uniform(rng_);
        Event e; e.id = id;
        e.a = {p*sin_theta*std::cos(phi), p*sin_theta*std::sin(phi), p*cos_theta, energy};
        e.b = {-e.a.px, -e.a.py, -e.a.pz, energy};
        e.q2 = uniform(rng_) < 0.15 ? 1 : -1; // Fondo combinatorio sintetico.
        return e;
    }
};
class Game {
    Lesson lesson_;
    bool demo_;
    Progress& progress_;
    Terminal& terminal_;
    World world_;
    Position player_;
    std::vector<Specimen> team_{{0, 100}};
    Specimen wild_{1, 100};
    bool encounter_ = false;
    int encounters_ = 0, moves_ = 0, events_ = 0, selected_ = 0;
    Generator generator_{42};
    Event latest_{};
    std::string message_ = "Explora el mapa. E abre un encuentro reproducible.";
    std::vector<Event> observations_;
    void award(const std::string& id) {
        if (demo_) return;
        if (progress_.award(id)) { message_ += " | LOGRO: " + id; terminal_.sound(); }
    }
    void meet() {
        wild_ = {static_cast<std::size_t>((++encounters_) % static_cast<int>(catalogue().size())), 100};
        encounter_ = true; latest_ = generator_.next(++events_);
        observations_.push_back(latest_);
        message_ = "Encuentro: elige una accion. El detector registra un evento sintetico independiente.";
        if (lesson_.select(latest_, 0.6)) { ++selected_; award("first_selection"); }
        terminal_.sound();
    }
    void save_events() {
        if (lesson_.number == 5 && check_lesson(lesson_) != 0) {
            message_ = "El escritor aun no conserva el evento. Completa y verifica S05 antes de exportar.";
            return;
        }
        std::filesystem::create_directories("output");
        std::ofstream out("output/encounters.csv");
        out << std::setprecision(17) << csv_header;
        for (const auto& e : observations_) lesson_.record(out, e, lesson_.select(e, 0.6));
        if (!out) throw std::runtime_error("No se pudo escribir output/encounters.csv");
        message_ = "CSV: output/encounters.csv | JSON: " + progress_.export_json(demo_).string();
        if (!observations_.empty()) award("first_export");
    }
public:
    Game(Lesson lesson, bool demo, Progress& progress, Terminal& terminal) :
        lesson_(std::move(lesson)), demo_(demo), progress_(progress), terminal_(terminal) {}
    void render() const {
        terminal_.clear(); terminal_.ink("[ ROOT QUEST / OBSERVATORY ]");
        std::cout << "  NIVEL 0" << lesson_.number << (demo_ ? "  [DEMO / sin logros]" : "  [TALLER]") << '\n';
        std::cout << lesson_.title << "\n" << lesson_.task << "\nArchivo: " << lesson_.source << "\n\n";
        for (int y = 0; y < static_cast<int>(world_.rows.size()); ++y) {
            std::cout << "  ";
            for (int x = 0; x < static_cast<int>(world_.rows[static_cast<std::size_t>(y)].size()); ++x) {
                const char tile = player_.x == x && player_.y == y ? '@' : world_.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                std::cout << tile << ' ';
            }
            std::cout << '\n';
        }
        std::cout << "\n  @ jugador | # pared | : zona de observacion\n";
        std::cout << "\nEQUIPO " << team_.size() << "/6   ";
        for (const auto& p : team_) std::cout << '[' << catalogue()[p.species].symbol << ':' << p.hp << "] ";
        if (encounter_) {
            const auto& s = catalogue()[wild_.species];
            std::cout << "\n\n     .---.     " << s.name << " / " << family_name(s.family)
                << "\n   --( " << std::setw(3) << s.symbol << " )--  vitalidad [";
            for (int i = 0; i < 10; ++i) std::cout << (i < wild_.hp/10 ? '#' : '.');
            std::cout << "] " << wild_.hp << "\n     '---'     carga = " << s.charge3 << "/3 e\n"
                << "\n [1] Interactuar  [2] Capturar  [3] Salir del encuentro\n";
        }
        std::cout << "\nDETECTOR [SINTETICO] eventos=" << events_ << " seleccionados=" << selected_;
        if (events_) std::cout << " ultimo m=" << std::fixed << std::setprecision(3) << latest_.mass() << " GeV";
        std::cout << "\nLOGROS " << progress_.count() << " | " << message_ << '\n';
        std::cout << "\n[WASD/flechas] Mover  [E] Encuentro  [V] Verificar nivel\n"
            << "[X] Exportar CSV/progreso  [Q] Guardar progreso y salir\n" << std::flush;
    }
    void run() {
        while (!interrupted) {
            render(); const int key = terminal_.key();
            if (key == 'q' || key == 'Q') break;
            if (key == 'v' || key == 'V') {
                const bool passed = check_lesson(lesson_) == 0;
                message_ = passed ? "Comprobacion correcta: explica por que antes de continuar." : "Hay casos pendientes. Inspecciona tu codigo con un breakpoint.";
                if (passed) award("check0" + std::to_string(lesson_.number));
            } else if (key == 'x' || key == 'X') save_events();
            else if (encounter_) {
                if (key == '1') {
                    wild_.hp = lesson_.damage(wild_.hp, 25);
                    message_ = "Vitalidad actual: " + std::to_string(wild_.hp) + ". Regla ludica, no tasa de decaimiento.";
                    if (wild_.hp == 0) { encounter_ = false; message_ += " Encuentro terminado."; }
                } else if (key == '2') {
                    if (lesson_.capture(team_, &wild_)) {
                        encounter_ = false; message_ = "Criatura incorporada al equipo."; award("first_capture");
                    } else message_ = "Captura rechazada: revisa vitalidad, capacidad o el codigo del nivel.";
                } else if (key == '3') { encounter_ = false; message_ = "De vuelta al mapa."; }
            } else if (key == 'e' || key == 'E') meet();
            else {
                Input input;
                if (key == 'w') input.dy = -1;
                if (key == 's') input.dy = 1;
                if (key == 'a') input.dx = -1;
                if (key == 'd') input.dx = 1;
                if (input.dx || input.dy) {
                    if (lesson_.move(player_, input, world_)) {
                        message_ = "Posicion: (" + std::to_string(player_.x) + ", " + std::to_string(player_.y) + ")";
                        award("first_move");
                        if (++moves_ % 5 == 0) meet();
                    } else message_ = "Movimiento rechazado. Predice: pared, limite o funcion pendiente?";
                }
            }
        }
        const auto path = progress_.export_json(demo_);
        std::cout << "\nProgreso: " << path.string() << "\nImportalo en la pagina del curso.\n";
    }
};
} // namespace course
