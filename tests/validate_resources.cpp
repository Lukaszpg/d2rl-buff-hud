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
    // <layout>... <buff-hud.txt>: every shipped layout (HD and legacy) must
    // carry the full BuffHud widget contract.
    if (argc < 3) return 2;
    try {
        std::string error;
        for (int i = 1; i + 1 < argc; ++i) {
            const auto layout = Read(argv[i]);
            if (!BuffPanel::Systems::BuffHud::LooseLayout::Validate(layout, error)) {
                std::cerr << argv[i] << ": " << error << '\n';
                return 1;
            }
        }
        auto table = Read(argv[argc - 1]);
        if (!BuffPanel::Systems::BuffTracker::LooseBuffHud::ValidateAndNormalize(table, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "PASS: companion layouts and buff table\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
