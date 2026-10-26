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
    art::SignalMotion signal_motion_;
    Generator generator_{42};
    Event latest_{};
    std::string message_ = "WASD explora los sectores. [E] abre un encuentro.";
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
        signal_motion_ = art::SignalMotion{};
        encounter_ = true; latest_ = generator_.next(++events_);
        observations_.push_back(latest_);
        message_ = "Encuentro #" + std::to_string(encounters_) + ": " + catalogue()[wild_.species].name + ".";
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
    static constexpr const char* dim = "dim";
    static constexpr const char* accent = "cyan";
    static constexpr const char* good = "good";
    static constexpr const char* pending = "amber";
    static constexpr const char* strong = "ink-bold";
    std::string paint(const char* code, const std::string& text) const {
        const std::string raw(code);
        const bool bold = raw == "ink-bold";
        const std::string name = bold ? "ink" : raw == "92" ? "good" : raw == "96" ? "cyan" :
            raw == "93" || raw == "91" || raw == "33" ? "amber" : raw == "90" ? "dim" :
            raw == "1;97" || raw == "1" ? "ink" : raw == "35" ? "region" : raw;
        const int depth = terminal_.color_depth();
        if (depth == 24) {
            const char* rgb = name == "cyan" ? "38;2;2;132;199" :
                name == "good" ? "38;2;16;185;129" : name == "amber" ? "38;2;217;119;6" :
                name == "region" ? "38;2;125;211;252" : name == "dim" ? "38;2;113;113;122" : "38;2;24;24;27";
            return "\033[" + std::string(rgb) + "m" + (bold ? "\033[1m" : "") + text + terminal_.paper();
        }
        if (depth == 256) {
            const char* index = name == "cyan" ? "32" : name == "good" ? "41" :
                name == "amber" ? "172" : name == "region" ? "117" : name == "dim" ? "245" : "232";
            return "\033[38;5;" + std::string(index) + "m" + (bold ? "\033[1m" : "") + text + terminal_.paper();
        }
        const char* ansi = name == "cyan" ? "36" : name == "good" ? "32" :
            name == "amber" ? "33" : name == "region" ? "36" : name == "dim" ? "90" : "1;30";
        return "\033[" + std::string(ansi) + "m" + (bold ? "\033[1m" : "") + text + terminal_.paper();
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
        std::string sessions;
        for (int i = 1; i <= 6; ++i) {
            const std::string number = (i < 10 ? "0" : "") + std::to_string(i);
            if (progress_.has("check0" + std::to_string(i))) sessions += paint(good, number) + " ";
            else if (i == lesson_.number) sessions += paint(accent, "[" + number + "]") + " ";
            else sessions += paint(dim, number) + " ";
        }
        const std::string left = paint(strong, "WIRED / OBSERVATORY");
        const std::string tail = paint(dim, right) + "  " + sessions;
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
    std::string wall_cell(int x, int y) const {
        const auto& rows = world_.rows;
        const char tile = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        if (player_.x == x && player_.y == y)
            return paint(accent, terminal_.unicode() ? "◆ " : "@ ");
        if (tile == ':') return paint(dim, terminal_.unicode() ? "· " : ": ");
        if (tile != '#') return "  ";

        const auto wall = [&](int nx, int ny) {
            return ny >= 0 && ny < static_cast<int>(rows.size()) && nx >= 0 &&
                nx < static_cast<int>(rows[static_cast<std::size_t>(ny)].size()) &&
                rows[static_cast<std::size_t>(ny)][static_cast<std::size_t>(nx)] == '#';
        };
        const int mask = (wall(x, y - 1) ? 1 : 0) | (wall(x + 1, y) ? 2 : 0) |
            (wall(x, y + 1) ? 4 : 0) | (wall(x - 1, y) ? 8 : 0);
        if (!terminal_.unicode()) {
            const bool vertical = (mask & 5) != 0, horizontal = (mask & 10) != 0;
            const char edge = vertical && horizontal ? '+' : vertical ? '|' : horizontal ? '-' : ' ';
            return std::string(1, edge) + (mask & 2 ? '-' : ' ');
        }
        static const char* junctions[] = {" ", "│", "─", "└", "│", "│", "┌", "├",
            "─", "┘", "─", "┴", "┐", "┤", "┬", "┼"};
        return std::string(junctions[mask]) + ((mask & 2) ? "─" : " ");
    }
    void render_checks(std::vector<std::string>& lines, bool compact, bool label = true, bool hint = true) const {
        const bool u = terminal_.unicode();
        if (!checked_) {
            lines.push_back((label ? paint(dim, "CODE_INSPECTOR  ") : "") + "[V] comprobar el ejercicio");
            return;
        }
        const auto passed = std::count_if(checks_.begin(), checks_.end(), [](const Check& c) { return c.ok; });
        const bool all = passed == static_cast<long>(checks_.size());
        lines.push_back((label ? paint(dim, "CODE_INSPECTOR  ") : "") + paint(all ? good : pending,
            std::to_string(passed) + "/" + std::to_string(checks_.size()))
            + (all ? paint(dim, "  listo: explica por que funciona") : paint(dim, "  edita, recompila y vuelve a [V]")));
        if (all) lines.back() += "  " + paint(good, u ? "✓" : "OK");
        if (!all && hint) {
            const auto next = std::find_if(checks_.begin(), checks_.end(), [](const Check& c) { return !c.ok; });
            if (next != checks_.end()) {
                const auto size = terminal_.dimensions();
                wrap(lines, std::string("  ") + (u ? "›" : ">") + " " + next->text,
                    std::min(72, size.columns - 2), compact ? 1 : 2);
            }
        } else if (!compact) lines.push_back(paint(dim, "  Nivel completo; revisa el resultado y explica el codigo."));
    }
    void present(std::vector<std::string> body, const std::string& footer, int rows) const {
        const auto count = static_cast<std::size_t>(std::max(0, rows - 1));
        body.resize(std::min(body.size(), count));
        body.resize(count);
        body.push_back(footer);
        terminal_.present(body);
    }
    void render_map(int columns, int rows) const {
        const bool compact = columns < 80 || rows < 24;
        const bool u = terminal_.unicode();
        const auto focus = lesson_focus(lesson_.number);
        std::vector<std::string> lines;
        lines.push_back(header(columns, demo_ ? "DEMO" : "TALLER"));
        lines.push_back(paint(strong, "S" + std::string(lesson_.number < 10 ? "0" : "") + std::to_string(lesson_.number) + "  " + lesson_.title));
        if (!compact) lines.push_back(rule(columns));
        lines.push_back(paint(dim, "TOPOLOGY_OBSERVATORY  /  " ) + "sectores de observacion");
        wrap(lines, std::string("Mision  ") + lesson_.task, columns - 2, 2);
        lines.push_back(paint(dim, "Codigo  ") + lesson_.source + "  " + paint(accent, focus.call));

        const bool narrow = columns < 80;
        const int map_width = static_cast<int>(world_.rows[0].size()) * (narrow ? 1 : 2);
        const int side_width = std::max(0, columns - map_width - 5);
        std::vector<std::string> side;
        side.push_back(paint(dim, "PARTICLE_REGISTRY") + " " + std::to_string(team_.size()) + "/6");
        std::string roster;
        for (const auto& p : team_) roster += (roster.empty() ? "" : " ") + std::string(catalogue()[p.species].symbol);
        std::vector<std::string> roster_lines;
        wrap(roster_lines, roster, side_width, 2);
        for (const auto& row : roster_lines) side.push_back(paint(good, row));
        const auto& companion = catalogue()[team_.back().species];
        side.push_back(rule(side_width));
        side.push_back(paint(dim, "Ultimo: ") + companion.name);
        side.push_back(paint(dim, std::string(family_name(companion.family)) + " q=" + charge_text(companion.charge3)
            + " HP juego=" + std::to_string(team_.back().hp)));
        side.push_back("Posicion " + std::to_string(player_.x) + ", " + std::to_string(player_.y));
        side.push_back(paint(dim, "EVENT_INSPECTOR"));
        side.push_back("evt:" + std::to_string(events_) + " sel:" + std::to_string(selected_));
        for (std::size_t y = 0; y < world_.rows.size(); ++y) {
            std::string row;
            for (std::size_t x = 0; x < world_.rows[y].size(); ++x) {
                if (narrow) {
                    const char tile = world_.rows[y][x];
                    row += player_.x == static_cast<int>(x) && player_.y == static_cast<int>(y) ? (u ? "◆" : "@") :
                        tile == '#' ? (u ? "·" : ".") : tile == ':' ? ":" : " ";
                } else row += wall_cell(static_cast<int>(x), static_cast<int>(y));
            }
            lines.push_back(pad(row, map_width) + "  " + (y < side.size() && cells(side[y]) <= side_width ? side[y] : ""));
        }
        lines.push_back(paint(dim, "OBSERVATORY_LOG  ") + "actividad reciente");
        wrap(lines, "> " + message_, columns - 2, 2);
        render_checks(lines, compact);
        present(lines, paint(accent, compact ? "[WASD] [E] encuentro [V] probar [X] exportar [Q] salir"
            : "[WASD] mover  [E] encuentro  [V] probar  [X] exportar  [Q] salir"), rows);
    }
    void render_encounter(int columns, int rows) const {
        const bool u = terminal_.unicode();
        const auto& s = catalogue()[wild_.species];
        const auto profile = art::signal_profile(wild_.species);
        const auto& metadata = art::signal_metadata[wild_.species];
        std::vector<std::string> lines;
        lines.push_back(header(columns, demo_ ? "DEMO" : "TALLER"));
        lines.push_back(paint(strong, "S" + std::string(lesson_.number < 10 ? "0" : "") + std::to_string(lesson_.number) + "  " + lesson_.title));
        lines.push_back(rule(columns));
        const int left_width = (columns - 5) / 2;
        const auto pair = [&](const std::string& left, const std::string& right) {
            lines.push_back(pad(left, left_width) + " " + paint(dim, u ? "│" : "|") + " " + right);
        };
        pair(paint(dim, "SPECIMEN_INSPECTOR"), paint(dim, "SPECIMEN_SIGNAL / " + std::string(profile.alternate ? "ALT Q/SPIN" : "LOG")));
        pair(paint(strong, std::string(s.name) + " / " + s.symbol), paint(profile.alternate ? pending : accent,
            std::string(profile.alternate ? "ALT" : "LOG") + " / base "
                + std::to_string(static_cast<int>(std::lround(signal_motion_.gain * 100.0))) + "%"));
        pair(std::string(family_name(s.family)) + " q=" + charge_text(s.charge3) + " spin=" + number(metadata.spin),
             "m=" + (metadata.mass_mev ? number(*metadata.mass_mev) + " MeV/c^2" : std::string("N/D")));
        const int plot_width = std::max(9, std::min(29, columns - left_width - 5));
        const auto plot = art::lissajous(profile, signal_motion_.phase, signal_motion_.gain,
                                        plot_width, columns < 80 ? 4 : 5, u);
        const auto& ally = catalogue()[team_.back().species];
        for (std::size_t i = 0; i < plot.size(); ++i) {
            std::string left;
            if (i == 0) left = "HP juego " + std::to_string(wild_.hp) + "/100";
            else if (i == 1) left = "captura: HP <= 50";
            else if (i == 2) left = "PARTICLE_REGISTRY " + std::to_string(team_.size()) + "/6";
            else if (i == 3) left = "Ultimo: " + std::string(ally.name) + " / " + ally.symbol;
            pair(left, paint(profile.alternate ? pending : accent, plot[i]));
        }
        pair(paint(dim, "EVENT_INSPECTOR"), "SINTETICO #" + std::to_string(latest_.id));
        pair("m " + number(latest_.mass()) + " GeV",
             "pT " + number(latest_.a.pt()) + "/" + number(latest_.b.pt()) + " GeV");
        pair("candidatos q " + signed_charge(latest_.q1) + "/" + signed_charge(latest_.q2),
             lesson_.number == 6 ? "in_window=" + std::string(latest_window_ ? "true" : "false")
                                 : "select=" + std::string(latest_selected_ ? "true" : "false"));
        pair("[1] Interactuar", "[2] Capturar   [3] Volver");
        if (columns >= 80) lines.push_back(rule(columns));
        const auto focus = lesson_focus(lesson_.number);
        lines.push_back(paint(dim, "CODE_INSPECTOR"));
        wrap(lines, std::string(lesson_.source) + "  " + focus.call, columns - 2, 1);
        render_checks(lines, true, false);
        lines.push_back(paint(dim, "OBSERVATORY_LOG"));
        wrap(lines, message_, columns - 2, 2);
        present(lines, paint(accent, "[1] [2] [3]  [V] probar  [X] exportar  [Q] salir"), rows);
    }
public:
    Game(Lesson lesson, bool demo, Progress& progress, Terminal& terminal) :
        lesson_(std::move(lesson)), demo_(demo), progress_(progress), terminal_(terminal) {}
    void render_plain() const {
        const auto& focus = lesson_focus(lesson_.number);
        terminal_.clear();
        std::cout << "WIRED / OBSERVATORY  S0" << lesson_.number << "  " << lesson_.title
            << (demo_ ? "  [DEMO / sin logros]" : "  [TALLER]") << "\nTOPOLOGY_OBSERVATORY / sectores\nMision: " << lesson_.task
            << "\nCodigo: " << lesson_.source << "  " << focus.call << ' ' << focus.where << "\n\n";
        if (encounter_) {
            const auto& s = catalogue()[wild_.species];
            const auto profile = art::signal_profile(wild_.species);
            const auto& metadata = art::signal_metadata[wild_.species];
            std::cout << "SPECIMEN_INSPECTOR / registro #" << encounters_ << "\n" << s.name << " (" << s.symbol << ", " << family_name(s.family)
                << "), carga " << charge_text(s.charge3) << "\nHP de juego " << wild_.hp << "/100; captura: HP <= 50 y espacio en registro\n"
                << "m=" << (metadata.mass_mev ? number(*metadata.mass_mev) + " MeV/c^2" : std::string("N/D"))
                << " spin=" << number(metadata.spin) << "\nEVENT_INSPECTOR / sintetico #" << latest_.id << " candidatos a/b\n m=" << number(latest_.mass())
                << " GeV  pT=" << number(latest_.a.pt()) << "/" << number(latest_.b.pt()) << " GeV\n";
            std::cout << "SPECIMEN_SIGNAL " << (profile.alternate ? "ALT Q/SPIN" : "LOG")
                << " a=" << number(profile.x_frequency) << " b=" << number(profile.y_frequency)
                << " spin=" << number(metadata.spin) << "\n";
            for (const auto& row : art::lissajous(profile, 0.0, 1.0, 31, 5)) std::cout << " " << row << '\n';
            std::cout << "EVENT_INSPECTOR / DATOS SINTETICOS\n";
            std::cout << "select -> " << (latest_selected_ ? "true" : "false") << "\n\n"
                << "[1] Interactuar -> damage(hp, 25)\n[2] Capturar    -> capture(team, &wild)\n[3] Huir\n";
        } else {
            for (int y = 0; y < static_cast<int>(world_.rows.size()); ++y) {
                std::cout << "  ";
                for (int x = 0; x < static_cast<int>(world_.rows[static_cast<std::size_t>(y)].size()); ++x)
                    std::cout << (player_.x == x && player_.y == y ? '@' : world_.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) << ' ';
                std::cout << '\n';
            }
            std::cout << "  @ posicion | # sectores | : zona de observacion\n";
        }
        std::cout << "\nPARTICLE_REGISTRY " << team_.size() << "/6:";
        for (const auto& p : team_) std::cout << ' ' << catalogue()[p.species].symbol << '(' << p.hp << ')';
        std::cout << "\nEVENT_INSPECTOR [DATOS SINTETICOS] eventos=" << events_ << " seleccionados=" << selected_ << '\n';
        if (!checked_) std::cout << "CODE_INSPECTOR: pulsa V para comprobar tu codigo\n";
        for (const auto& c : checks_) std::cout << (c.ok ? "  [OK]   " : "  [TODO] ") << c.text << '\n';
        std::cout << "> " << message_ << "\n\n"
            << (encounter_ ? "[1] [2] [3]" : "[WASD] Mover  [E] Encuentro")
            << "  [V] Probar  [X] Exportar  [Q] Salir\n" << std::flush;
    }
    void render() const {
        if (!terminal_.rich()) { render_plain(); return; }
        const auto size = terminal_.dimensions();
        if (size.columns < 60 || size.rows < 21) {
            terminal_.present({"WIRED / OBSERVATORY", "Consola reducida: " + std::to_string(size.columns) + "x" + std::to_string(size.rows),
                "Amplia a 60x21; recomendado 80x24.", "[Q] Guardar y salir | --plain --step: modo texto"});
            return;
        }
        if (encounter_) render_encounter(size.columns, size.rows);
        else render_map(size.columns, size.rows);
    }
    std::filesystem::path run() {
        auto previous_tick = std::chrono::steady_clock::now();
        while (!interrupted) {
            render();
            const bool animate = terminal_.rich() && encounter_;
            const int key = terminal_.key(animate ? 90 : -1);
            if (animate) {
                const auto now = std::chrono::steady_clock::now();
                signal_motion_.advance(std::chrono::duration<double>(now - previous_tick).count());
                previous_tick = now;
            }
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
                    if (wild_.hp < before) signal_motion_.impact();
                    message_ = "damage(" + std::to_string(before) + ", 25) devolvio " + std::to_string(wild_.hp) + ".";
                    if (wild_.hp == 0) { encounter_ = false; message_ += " El especimen se retira."; }
                    else if (wild_.hp <= 50) message_ += " Ya puedes intentar [2] Capturar.";
                } else if (key == '2') {
                    if (lesson_.capture(team_, &wild_)) {
                        encounter_ = false; message_ = "Especimen incorporado al registro."; award("first_capture");
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
