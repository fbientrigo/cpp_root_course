#pragma once
#include <algorithm>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TTree.h>
#include <Math/Vector4D.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace analysis {
struct Row {
    int event_id, q1, q2, selected;
    double px1, py1, pz1, E1, px2, py2, pz2, E2;
    double pt1() const { return std::hypot(px1, py1); }
    double pt2() const { return std::hypot(px2, py2); }
    double mass() const {
        ROOT::Math::PxPyPzEVector a(px1, py1, pz1, E1), b(px2, py2, pz2, E2);
        return (a + b).M();
    }
};
inline std::vector<Row> read(int level) {
    const auto filename = "output/events-s0" + std::to_string(level) + ".csv";
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Genera los datos antes: python3 course.py batch " + std::to_string(level) + " --demo");
    std::string line;
    if (!std::getline(in, line) || line != "event_id,px1,py1,pz1,E1,px2,py2,pz2,E2,q1,q2,selected")
        throw std::runtime_error("El esquema CSV no coincide con el contrato.");
    std::vector<Row> result;
    int number = 1;
    while (std::getline(in, line)) {
        ++number;
        for (auto& c : line) if (c == ',') c = ' ';
        std::istringstream stream(line); Row r{}; std::string extra;
        if (!(stream >> r.event_id >> r.px1 >> r.py1 >> r.pz1 >> r.E1 >> r.px2 >> r.py2 >> r.pz2 >> r.E2 >> r.q1 >> r.q2 >> r.selected) || stream >> extra)
            throw std::runtime_error("Fila CSV invalida: " + std::to_string(number));
        result.push_back(r);
    }
    if (result.empty()) throw std::runtime_error("No hay eventos. Verifica el escritor de S05.");
    return result;
}
inline void inspect_tree(const char* path) {
    std::unique_ptr<TFile> file(TFile::Open(path));
    if (!file || file->IsZombie()) throw std::runtime_error("No se pudo abrir el TFile.");
    TTree* tree = nullptr; file->GetObject("events", tree);
    if (!tree) throw std::runtime_error("No existe el TTree events.");
    tree->Print();
    double px1 = 0; tree->SetBranchAddress("px1", &px1);
    for (Long64_t i = 0; i < std::min<Long64_t>(5, tree->GetEntries()); ++i) {
        tree->GetEntry(i); std::cout << "entry=" << i << " px1=" << px1 << '\n';
    }
    tree->ResetBranchAddresses();
}
}
