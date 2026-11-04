#include "systems/buff_tracker/buff_display_table.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: buff_panel_validate_display <buff-hud-display.txt>\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "cannot read " << argv[1] << '\n';
        return 1;
    }
    std::string text(std::istreambuf_iterator<char>(input), {});
    std::string error;
    if (!BuffPanel::Systems::BuffTracker::DisplayTable::ValidateAndNormalize(text, error)) {
        std::cerr << argv[1] << ": " << error << '\n';
        return 1;
    }
    std::cout << "PASS: " << argv[1] << '\n';
    return 0;
}
