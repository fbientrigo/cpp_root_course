#include "common.hpp"
void s02() {
    const auto rows = analysis::read(2);
    auto canvas = new TCanvas("s02_canvas", "S02: estado de un objeto", 850, 500);
    auto h = new TH1D("s02_mass", "Modelo sintetico;m_{#mu#mu} [GeV];Eventos", 100, 0, 6);
    for (const auto& event : rows) h->Fill(event.mass());
    std::cout << "Instancias: " << h->GetEntries() << " eventos. Media=" << h->GetMean() << '\n';
    h->Draw(); canvas->Update();
    // Pregunta: que metodos modifican h y cuales solo consultan su estado?
}
