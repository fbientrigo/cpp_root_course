#include "common.hpp"
void s04() {
    auto rows = analysis::read(4);
    auto canvas = new TCanvas("s04_canvas", "S04: efecto de una seleccion", 900, 500);
    auto before = new TH1D("s04_before", "Modelo sintetico: antes/despues;m_{#mu#mu} [GeV];Eventos", 100, 0, 6);
    auto after = new TH1D("s04_after", "Seleccionados", 100, 0, 6);
    for (const auto& event : rows) { before->Fill(event.mass()); if (event.selected) after->Fill(event.mass()); }
    after->SetLineColor(kBlue + 1); before->Draw(); after->Draw("SAME"); canvas->BuildLegend(); canvas->Update();
    // Cambia el criterio C++, vuelve a generar con la misma semilla y compara.
}
