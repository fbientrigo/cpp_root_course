#pragma once
#include "lesson.hpp"
#include "terminal.hpp"
#include "sprites.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
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
    bool has(const std::string& id) const { return earned_.count(id) != 0; }
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
// Lo que el alumno escribe en cada nivel y donde lo ve actuar dentro del juego.
struct Focus { const char* call; const char* where; };
inline Focus lesson_focus(int number) {
    static const Focus focus[] = {
        {"move(player, input, world)", "al moverte con WASD"},
        {"damage(hp, amount)", "en [1] Interactuar"},
        {"capture(team, wild)", "en [2] Capturar"},
        {"select(event, cut)", "en el detector"},
        {"record(out, event, selected)", "en [X] Exportar"},
        {"in_window(mass, low, high)", "en el detector"}};
    return focus[std::clamp(number, 1, 6) - 1];
}
inline std::string charge_text(int charge3) {
    if (charge3 == 0) return "0 e";
    const int n = std::abs(charge3);
    return (charge3 > 0 ? "+" : "-") + (n % 3 == 0 ? std::to_string(n / 3) : std::to_string(n) + "/3") + " e";
}
class Game {
    struct Check { bool ok; std::string text; };
    Lesson lesson_;
    bool demo_;
    Progress& progress_;
    Terminal& terminal_;
    World world_;
    Position player_;
    std::vector<Specimen> team_{{0, 100}};
    Specimen wild_{1, 100};
    bool encounter_ = false, latest_selected_ = false, latest_window_ = false;
    int encounters_ = 0, moves_ = 0, events_ = 0, selected_ = 0;
    Generator generator_{42};
    Event latest_{};
    std::string message_ = "Muevete con WASD. Cada 5 pasos, o con E, aparece una criatura.";
    std::vector<Event> observations_;
    // Vacio hasta el primer [V]: el codigo del alumno puede fallar y no debe
    // ejecutarse sin que lo pida.
    std::vector<Check> checks_;
    bool checked_ = false;
    int verify() {
        std::ostringstream report;
        const int result = check_lesson(lesson_, report);
        checks_.clear();
        std::istringstream lines(report.str());
        for (std::string line; std::getline(lines, line);) {
            const auto marker = line.find(']');
            if (marker == std::string::npos) continue;
            const auto text = line.find_first_not_of(' ', marker + 1);
            checks_.push_back({line.rfind("[OK]", 0) == 0, text == std::string::npos ? "" : line.substr(text)});
        }
        checked_ = true;
        return result;
    }
    void award(const std::string& id) {
        if (demo_) return;
        if (progress_.award(id)) { message_ += " | logro: " + id; terminal_.sound(); }
    }
    void meet() {
        wild_ = {static_cast<std::size_t>((++encounters_) % static_cast<int>(catalogue().size())), 100};
        encounter_ = true; latest_ = generator_.next(++events_);
        observations_.push_back(latest_);
        message_ = std::string("Aparecio un ") + catalogue()[wild_.species].name + " salvaje. Elige una accion.";
        latest_selected_ = lesson_.select(latest_, 0.6);
        if (lesson_.number == 6) latest_window_ = lesson_.in_window(latest_.mass(), 3.0, 3.2);
        if (latest_selected_) { ++selected_; award("first_selection"); }
        terminal_.sound();
    }
    void save_events() {
        if (lesson_.number == 5 && verify() != 0) {
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

    // Interfaz: pocos colores, cada uno con un significado fijo.
    static constexpr const char* dim = "90";
    static constexpr const char* accent = "96";
    static constexpr const char* good = "92";
    static constexpr const char* pending = "93";
    static constexpr const char* strong = "1;97";
    static std::string paint(const char* code, const std::string& text) {
        return "\033[" + std::string(code) + "m" + text + "\033[0;37;40m";
    }
    static int cells(const std::string& text) {
        int n = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const auto c = static_cast<unsigned char>(text[i]);
            if (c == 27 && i + 1 < text.size() && text[i + 1] == '[') {
                for (i += 2; i < text.size() && !(text[i] >= 0x40 && text[i] <= 0x7e); ++i) {}
            } else if ((c & 0xc0) != 0x80) ++n;
        }
        return n;
    }
    static std::string pad(std::string text, int width) {
        const int n = cells(text);
        return n < width ? text + std::string(static_cast<std::size_t>(width - n), ' ') : text;
    }
    static void wrap(std::vector<std::string>& lines, std::string text, int width, int max_lines) {
        const auto limit = static_cast<std::size_t>(std::max(1, width));
        for (int i = 0; i < max_lines && !text.empty(); ++i) {
            auto end = text.size() <= limit ? text.size() : text.rfind(' ', limit);
            if (end == std::string::npos || end == 0) end = limit;
            lines.push_back(text.substr(0, end));
            text.erase(0, std::min(end + 1, text.size()));
        }
    }
    static std::string signed_charge(int q) { return (q > 0 ? "+" : "") + std::to_string(q); }
    static std::string number(double n) {
        std::ostringstream out; out << std::fixed << std::setprecision(3) << n; return out.str();
    }
    std::string header(int columns, const std::string& right) const {
        const bool u = terminal_.unicode();
        std::string sessions;
        for (int i = 1; i <= 6; ++i) {
            if (progress_.has("check0" + std::to_string(i))) sessions += paint(good, u ? "●" : "#");
            else if (i == lesson_.number) sessions += paint(accent, u ? "○" : "o");
            else sessions += paint(dim, u ? "·" : ".");
        }
        const std::string left = paint(strong, "ROOT QUEST") + "  " + paint(accent, "S0" + std::to_string(lesson_.number))
            + "  " + lesson_.title;
        const std::string tail = sessions + "  " + paint(dim, right);
        return pad(left, columns - 1 - cells(tail)) + tail;
    }
    std::string rule(int columns) const {
        std::string line;
        for (int i = 0; i < columns - 1; ++i) line += terminal_.unicode() ? "─" : "-";
        return paint(dim, line);
    }
    std::string mark_if(int lesson) const {
        return lesson_.number == lesson ? "  " + paint(accent, terminal_.unicode() ? "◂ tu codigo" : "<- tu codigo") : "";
    }
    std::vector<std::string> sprite_or_symbol(std::size_t species) const {
        if (terminal_.unicode()) return art::sprite(species);
        std::vector<std::string> rows(6, std::string(16, ' '));
        rows[2] = pad("     [" + std::string(catalogue()[species].symbol) + "]", 16);
        return rows;
    }
    void render_checks(std::vector<std::string>& lines, bool compact) const {
        const bool u = terminal_.unicode();
        if (!checked_) {
            lines.push_back(paint(dim, "PRUEBAS  ") + "pulsa [V] para comprobar tu codigo");
            return;
        }
        const auto passed = std::count_if(checks_.begin(), checks_.end(), [](const Check& c) { return c.ok; });
        const bool all = passed == static_cast<long>(checks_.size());
        lines.push_back(paint(dim, "PRUEBAS  ") + paint(all ? good : pending,
            std::to_string(passed) + "/" + std::to_string(checks_.size()))
            + (all ? paint(dim, "  listo: explica por que funciona") : paint(dim, "  edita, recompila y vuelve a [V]")));
        for (const auto& c : checks_) {
            if (compact && c.ok) continue;
            lines.push_back("  " + (c.ok ? paint(good, u ? "✓" : "OK") : paint(pending, u ? "✗" : "--")) + " " + c.text);
            if (compact) break; // En pantallas pequenas, solo la siguiente tarea.
        }
    }
    void present(std::vector<std::string> body, const std::string& footer, int rows) const {
        // El pie con las teclas siempre queda visible.
        body.resize(std::min(body.size(), static_cast<std::size_t>(std::max(0, rows - 1))));
        body.push_back(footer);
        terminal_.present(body);
    }
    void render_map(int columns, int rows) const {
        const bool compact = columns < 80 || rows < 24;
        const bool u = terminal_.unicode();
        const auto focus = lesson_focus(lesson_.number);
        std::vector<std::string> lines;
        lines.push_back(header(columns, demo_ ? "DEMO" : "TALLER"));
        if (!compact) lines.push_back(rule(columns));
        if (compact) {
            wrap(lines, std::string("MISION  ") + lesson_.task, columns - 2, 2);
            lines.push_back(paint(dim, "CODIGO  ") + lesson_.source);
            lines.push_back("        " + paint(accent, focus.call) + " " + paint(dim, focus.where));
        } else {
            lines.push_back(paint(dim, "MISION  ") + lesson_.task);
            lines.push_back(paint(dim, "CODIGO  ") + lesson_.source + "  " + paint(accent, focus.call) + " " + paint(dim, focus.where));
            lines.push_back("");
        }

        const auto& companion = team_.back();
        const auto& species = catalogue()[companion.species];
        auto side = sprite_or_symbol(companion.species);
        side.push_back(paint(strong, species.name) + paint(dim, "  " + std::string(family_name(species.family))));
        std::string team = "EQUIPO " + std::to_string(team_.size()) + "/6 ";
        for (const auto& p : team_) team += " " + std::string(catalogue()[p.species].symbol);
        side.push_back(team);
        side.push_back(paint(dim, "detector " + std::to_string(events_) + " evt, " + std::to_string(selected_) + " sel"));

        const int map_width = static_cast<int>(world_.rows[0].size()) * (compact ? 1 : 2) + (compact ? 3 : 4);
        for (std::size_t y = 0; y < world_.rows.size(); ++y) {
            std::string row;
            for (std::size_t x = 0; x < world_.rows[y].size(); ++x) {
                const char tile = world_.rows[y][x];
                if (player_.x == static_cast<int>(x) && player_.y == static_cast<int>(y))
                    row += paint(pending, u ? (compact ? "◆" : "▐▌") : (compact ? "@" : "@ "));
                else if (tile == '#') row += paint(dim, u ? (compact ? "█" : "██") : (compact ? "#" : "##"));
                else if (tile == ':') row += paint("35", u ? (compact ? "░" : "░░") : (compact ? ":" : ": "));
                else row += compact ? " " : "  ";
            }
            lines.push_back(pad(row, map_width) + (y < side.size() ? side[y] : ""));
        }
        if (!compact) lines.push_back(paint(dim, u ? "▐▌ tu   ░░ zona de observacion   ██ pared" : "@ tu   : zona de observacion   # pared"));
        wrap(lines, "> " + message_, columns - 2, 2);
        if (!compact) lines.push_back("");
        render_checks(lines, compact);
        present(lines, paint(accent, compact ? "[WASD] [E] encuentro [V] probar [X] exportar [Q] salir"
            : "[WASD] mover  [E] encuentro  [V] probar  [X] exportar  [Q] salir"), rows);
    }
    void render_encounter(int columns, int rows) const {
        const bool u = terminal_.unicode();
        const auto& s = catalogue()[wild_.species];
        std::vector<std::string> lines;
        lines.push_back(header(columns, "ENCUENTRO " + std::to_string(encounters_)));
        lines.push_back(rule(columns));
        lines.push_back("");

        // Disposicion Pokemon: ficha a la izquierda, criatura a la derecha.
        std::string bar;
        const int filled = std::clamp(wild_.hp / 5, 0, 20);
        for (int i = 0; i < 20; ++i) bar += i < filled ? (u ? "█" : "#") : (u ? "░" : "-");
        const char* health = wild_.hp > 50 ? good : wild_.hp > 25 ? pending : "91";
        const std::vector<std::string> card = {
            "  " + paint(strong, s.name),
            "  " + paint(dim, std::string(family_name(s.family)) + "  carga ") + charge_text(s.charge3),
            "",
            "  HP " + paint(health, bar) + " " + std::to_string(wild_.hp) + "/100",
            "",
            "  " + paint(dim, "tu companero: " + std::string(catalogue()[team_.back().species].name))};
        const auto pixels = sprite_or_symbol(wild_.species);
        const int card_width = std::max(40, columns - 22);
        for (std::size_t y = 0; y < pixels.size(); ++y) lines.push_back(pad(card[y], card_width) + pixels[y]);
        lines.push_back("");

        lines.push_back(paint(dim, "DETECTOR  datos sinteticos, evento #" + std::to_string(latest_.id)));
        lines.push_back("  m = " + number(latest_.mass()) + " GeV   pT " + number(latest_.a.pt()) + " / "
            + number(latest_.b.pt()) + " GeV   q " + signed_charge(latest_.q1) + " / " + signed_charge(latest_.q2));
        std::string verdict = "  select(event, 0.6) -> " + std::string(latest_selected_ ? "true" : "false") + mark_if(4);
        if (lesson_.number == 6)
            verdict = "  in_window(m, 3.0, 3.2) -> " + std::string(latest_window_ ? "true" : "false") + mark_if(6);
        lines.push_back(verdict);
        lines.push_back("");

        lines.push_back("  [1] Interactuar   " + paint(dim, "damage(hp, 25)") + mark_if(2));
        lines.push_back("  [2] Capturar      " + paint(dim, "capture(team, &wild)") + mark_if(3));
        lines.push_back("  [3] Huir          " + paint(dim, "volver al mapa"));
        lines.push_back("");
        wrap(lines, "> " + message_, columns - 2, 2);
        present(lines, paint(accent, "[1] [2] [3]  [V] probar  [X] exportar  [Q] salir"), rows);
    }
public:
    Game(Lesson lesson, bool demo, Progress& progress, Terminal& terminal) :
        lesson_(std::move(lesson)), demo_(demo), progress_(progress), terminal_(terminal) {}
    void render_plain() const {
        const auto& focus = lesson_focus(lesson_.number);
        terminal_.clear();
        std::cout << "ROOT QUEST - S0" << lesson_.number << ' ' << lesson_.title
            << (demo_ ? "  [DEMO / sin logros]" : "  [TALLER]") << "\nMISION: " << lesson_.task
            << "\nCODIGO: " << lesson_.source << " -> " << focus.call << ' ' << focus.where << "\n\n";
        if (encounter_) {
            const auto& s = catalogue()[wild_.species];
            std::cout << "ENCUENTRO " << encounters_ << ": " << s.name << " (" << family_name(s.family)
                << "), carga " << charge_text(s.charge3) << "\nHP [";
            for (int i = 0; i < 10; ++i) std::cout << (i < wild_.hp / 10 ? '#' : '-');
            std::cout << "] " << wild_.hp << "/100\n"
                << "DETECTOR [SINTETICO] #" << latest_.id << " m=" << number(latest_.mass())
                << " GeV  select -> " << (latest_selected_ ? "true" : "false") << "\n\n"
                << "[1] Interactuar -> damage(hp, 25)\n[2] Capturar    -> capture(team, &wild)\n[3] Huir\n";
        } else {
            for (int y = 0; y < static_cast<int>(world_.rows.size()); ++y) {
                std::cout << "  ";
                for (int x = 0; x < static_cast<int>(world_.rows[static_cast<std::size_t>(y)].size()); ++x)
                    std::cout << (player_.x == x && player_.y == y ? '@' : world_.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) << ' ';
                std::cout << '\n';
            }
            std::cout << "  @ tu | # pared | : zona de observacion\n";
        }
        std::cout << "\nEQUIPO " << team_.size() << "/6:";
        for (const auto& p : team_) std::cout << ' ' << catalogue()[p.species].symbol << '(' << p.hp << ')';
        std::cout << "\nDETECTOR [SINTETICO] eventos=" << events_ << " seleccionados=" << selected_ << '\n';
        if (!checked_) std::cout << "PRUEBAS: pulsa V para comprobar tu codigo\n";
        for (const auto& c : checks_) std::cout << (c.ok ? "  [OK]   " : "  [TODO] ") << c.text << '\n';
        std::cout << "> " << message_ << "\n\n"
            << (encounter_ ? "[1] [2] [3]" : "[WASD] Mover  [E] Encuentro")
            << "  [V] Probar  [X] Exportar  [Q] Salir\n" << std::flush;
    }
    void render() const {
        if (!terminal_.rich()) { render_plain(); return; }
        const auto size = terminal_.dimensions();
        if (size.columns < 60 || size.rows < 21) {
            terminal_.present({"ROOT QUEST", "Consola reducida: " + std::to_string(size.columns) + "x" + std::to_string(size.rows),
                "Amplia a 60x21; recomendado 80x24.", "[Q] Guardar y salir | --plain --step: modo texto"});
            return;
        }
        if (encounter_) render_encounter(size.columns, size.rows);
        else render_map(size.columns, size.rows);
    }
    std::filesystem::path run() {
        while (!interrupted) {
            render(); const int key = terminal_.key();
            if (key == 'q' || key == 'Q') break;
            if (key == 'v' || key == 'V') {
                const bool passed = verify() == 0;
                const auto next = std::find_if(checks_.begin(), checks_.end(), [](const Check& c) { return !c.ok; });
                message_ = passed ? "Todas las pruebas pasan. Explica en voz alta por que funciona."
                    : "Siguiente: " + (next == checks_.end() ? std::string("revisa la salida") : next->text)
                        + ". Pon un breakpoint en " + lesson_.source + ".";
                if (passed) award("check0" + std::to_string(lesson_.number));
            } else if (key == 'x' || key == 'X') save_events();
            else if (encounter_) {
                if (key == '1') {
                    const int before = wild_.hp;
                    wild_.hp = lesson_.damage(wild_.hp, 25);
                    message_ = "damage(" + std::to_string(before) + ", 25) devolvio " + std::to_string(wild_.hp) + ".";
                    if (wild_.hp == 0) { encounter_ = false; message_ += " La criatura se retira."; }
                    else if (wild_.hp <= 50) message_ += " Ya puedes intentar [2] Capturar.";
                } else if (key == '2') {
                    if (lesson_.capture(team_, &wild_)) {
                        encounter_ = false; message_ = "Criatura incorporada al equipo."; award("first_capture");
                    } else message_ = "capture() devolvio false: HP sobre 50, equipo lleno o funcion pendiente.";
                } else if (key == '3') { encounter_ = false; message_ = "Volviste al mapa."; }
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
                    } else message_ = "move() devolvio false. Predice: pared, borde o funcion pendiente?";
                }
            }
        }
        return progress_.export_json(demo_);
    }
};
} // namespace course
