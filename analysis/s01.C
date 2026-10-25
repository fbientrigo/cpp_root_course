#include "common.hpp"
void s01() {
    auto rows = analysis::read(1);
    auto canvas = new TCanvas("s01_canvas", "S01: primer objeto ROOT", 850, 500);
    auto h = new TH1D("s01_pt", "Modelo sintetico;p_{T,1} [GeV];Eventos", 60, 0, 3);
    for (const auto& event : rows) h->Fill(event.pt1());
    h->Draw(); canvas->Update();
    // Explora h-> y canvas-> con el editor. Cambia binning, titulo y rango.
}
