#include "common.hpp"
void s03() {
    auto rows = analysis::read(3);
    auto canvas = new TCanvas("s03_canvas", "S03: colecciones", 1000, 450);
    canvas->Divide(2, 1);
    auto h = new TH1D("s03_pt", "Modelo sintetico;p_{T,1} [GeV];Eventos", 60, 0, 3);
    auto correlation = new TH2D("s03_corr", "Modelo en CM: correlacion impuesta;p_{T,1} [GeV];p_{T,2} [GeV]", 40, 0, 3, 40, 0, 3);
    for (const auto& event : rows) { h->Fill(event.pt1()); correlation->Fill(event.pt1(), event.pt2()); }
    canvas->cd(1); h->Draw(); canvas->cd(2); correlation->Draw("COLZ"); canvas->Update();
    // La diagonal se debe al generador de dos cuerpos en reposo, no a una ley de todo detector.
}
