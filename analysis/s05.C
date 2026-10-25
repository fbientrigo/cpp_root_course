#include "common.hpp"
void s05() {
    const auto rows = analysis::read(5);
    {
        TFile file("output/course.root", "RECREATE");
        if (file.IsZombie()) throw std::runtime_error("No se pudo crear output/course.root");
        analysis::Row r{};
        TTree tree("events", "Pares sinteticos; un evento por entrada");
        tree.Branch("event_id", &r.event_id, "event_id/I");
        tree.Branch("px1", &r.px1, "px1/D"); tree.Branch("py1", &r.py1, "py1/D");
        tree.Branch("pz1", &r.pz1, "pz1/D"); tree.Branch("E1", &r.E1, "E1/D");
        tree.Branch("px2", &r.px2, "px2/D"); tree.Branch("py2", &r.py2, "py2/D");
        tree.Branch("pz2", &r.pz2, "pz2/D"); tree.Branch("E2", &r.E2, "E2/D");
        tree.Branch("q1", &r.q1, "q1/I"); tree.Branch("q2", &r.q2, "q2/I");
        tree.Branch("selected", &r.selected, "selected/I");
        for (const auto& event : rows) { r = event; tree.Fill(); }
        if (tree.Write() <= 0) throw std::runtime_error("No se pudo escribir el TTree.");
    }
    analysis::inspect_tree("output/course.root");
    // En la consola: TFile f("output/course.root"); f.ls();
}
