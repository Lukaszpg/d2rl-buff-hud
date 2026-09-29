#include "systems/buff_hud/loose_layout.hpp"
#include "systems/buff_tracker/loose_buff_hud.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error(std::string("Cannot read ") + path);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        auto layout = Read(argv[1]);
        auto table = Read(argv[2]);
        std::string error;
        if (!BuffPanel::Systems::BuffHud::LooseLayout::Validate(layout, error)
            || !BuffPanel::Systems::BuffTracker::LooseBuffHud::ValidateAndNormalize(table, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "PASS: companion layout and buff table\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
