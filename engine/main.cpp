#include "game.hpp"
#include <limits>

int main(int argc, char** argv) {
    try {
        bool demo = false, check = false, step = false, plain = false, sound = false;
        int batch = 0; unsigned seed = 42; std::string student = "alumno";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (++i >= argc) throw std::runtime_error("Falta el valor de " + arg);
                return argv[i];
            };
            if (arg == "--demo") demo = true;
            else if (arg == "--check") check = true;
            else if (arg == "--step") step = true;
            else if (arg == "--plain") plain = true;
            else if (arg == "--sound") sound = true;
            else if (arg == "--student") student = value();
            else if (arg == "--batch") {
                const auto text = value(); std::size_t used = 0;
                batch = std::stoi(text, &used);
                if (used != text.size() || batch < 1 || batch > 1000000) throw std::runtime_error("Batch: entre 1 y 1000000 eventos.");
            } else if (arg == "--seed") {
                const auto text = value(); std::size_t used = 0;
                const auto n = std::stoul(text, &used);
                if (used != text.size() || text.front() == '-' || n > std::numeric_limits<unsigned>::max()) throw std::runtime_error("Semilla invalida.");
                seed = static_cast<unsigned>(n);
            } else if (arg == "--help") {
                std::cout << "ROOT QUEST: [--demo] [--check] [--step] [--plain] [--sound]\n"
                    << "[--student alias] [--batch N --seed 42]\n";
                return 0;
            } else throw std::runtime_error("Opcion desconocida: " + arg);
        }
        const auto lesson = course::make_lesson(demo);
        course::Progress progress(student, demo);
        if (check) {
            const int result = course::check_lesson(lesson);
            if (result == 0 && !demo) progress.award("check0" + std::to_string(lesson.number));
            progress.export_json(demo);
            return result;
        }
        if (batch) {
            if (lesson.number == 5 && course::check_lesson(lesson) != 0) return 1;
            std::filesystem::create_directories("output");
            const auto path = std::filesystem::path("output") / ("events-s0" + std::to_string(lesson.number) + ".csv");
            std::ofstream out(path); out << std::setprecision(17) << course::csv_header;
            course::Generator generator(seed); int selected = 0;
            for (int i = 0; i < batch; ++i) {
                const auto e = generator.next(i);
                const bool keep = lesson.select(e, 0.6) && (lesson.number != 6 || lesson.in_window(e.mass(), 3.0, 3.2));
                lesson.record(out, e, keep); if (keep) ++selected;
            }
            if (!out) throw std::runtime_error("No se pudo escribir el CSV.");
            std::cout << "Datos SINTETICOS: " << path.string() << " | solicitados=" << batch
                << " seleccionados=" << selected << " | seed=" << seed << '\n';
            return 0;
        }
        course::Terminal terminal(step, plain, sound);
        course::Game game(lesson, demo, progress, terminal);
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << '\n'; return 2;
    }
}
