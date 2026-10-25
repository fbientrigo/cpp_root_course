#include "common.hpp"
void s06() {
    const auto rows = analysis::read(6);
    auto canvas = new TCanvas("s06_canvas", "S06: informe sintetico", 1100, 500);
    canvas->Divide(2, 1);
    auto all = new TH1D("s06_all", "Muestra completa (sintetica);m_{#mu#mu} [GeV];Eventos", 120, 0, 6);
    auto kept = new TH1D("s06_kept", "Ventana elegida (sintetica);m_{#mu#mu} [GeV];Eventos", 120, 0, 6);
    for (const auto& event : rows) { all->Fill(event.mass()); if (event.selected) kept->Fill(event.mass()); }
    canvas->cd(1); all->Draw(); canvas->cd(2); kept->Draw(); canvas->Update();
    canvas->SaveAs("output/report.pdf");
    std::cout << "Seleccionados: " << kept->GetEntries() << '/' << all->GetEntries()
        << ". Una ventana es una seleccion, no una prueba de significancia.\n";
}
