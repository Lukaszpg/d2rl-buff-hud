#include "core/buff_presentation.hpp"
#include "systems/buff_tracker/buff_display_table.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace D = BuffPanel::Systems::BuffTracker::DisplayTable;
namespace Core = BuffPanel::Core;

D::Row MakeRow(
    std::uint32_t state, const char* atlas, std::uint16_t frame, const char* key, const char* name,
    const char* item = "") {
    D::Row row{};
    row.stateId = state;
    std::strncpy(row.iconAtlas, atlas, sizeof(row.iconAtlas) - 1);
    row.iconFrame = frame;
    std::strncpy(row.nameKey, key, sizeof(row.nameKey) - 1);
    std::strncpy(row.name, name, sizeof(row.name) - 1);
    std::strncpy(row.itemCode, item, sizeof(row.itemCode) - 1);
    return row;
}

int main(int argc, char** argv) {
    std::string error;

    // The shipped companion default must be valid (header-only means no overrides).
    assert(argc == 2);
    {
        std::ifstream input(argv[1], std::ios::binary);
        assert(input.good());
        std::string shipped(std::istreambuf_iterator<char>(input), {});
        assert(D::ValidateAndNormalize(shipped, error));
    }

    Core::BuffIconAtlas atlas{};
    assert(D::ParseAtlas("", atlas) && atlas == Core::BuffIconAtlas::Auto);
    assert(D::ParseAtlas("Paladin", atlas) && atlas == Core::BuffIconAtlas::Paladin);
    assert(D::ParseAtlas("global", atlas) && atlas == Core::BuffIconAtlas::Global);
    assert(!D::ParseAtlas("auto", atlas));
    assert(!D::ParseAtlas("pal", atlas));

    const std::string header = std::string(D::Header) + "\n";
    auto text = header
        + "32\tbarbarian\t26\t\t\t\r\n"              // icon only (stock Battle Orders state)
        + "26\t\t\t\tWar Cry\t\n"                     // name only, CRLF normalized
        + "\n"
        + "350\tglobal\t0\td2r:SomeKey\tFallback\thp5\n"; // frame 0 is a real frame
    assert(D::ValidateAndNormalize(text, error));
    assert(text.find('\r') == std::string::npos);

    for (const auto* invalid : {
             "0\tglobal\t0\t\t\t\n",            // state 0
             "4096\tglobal\t0\t\t\t\n",         // state out of range
             "32\tbarb\t0\t\t\t\n",             // unknown atlas
             "32\t\t5\t\t\t\n",                 // frame without atlas
             "32\tglobal\t65535\t\t\t\n",       // reserved automatic frame
             "32\tglobal\t70000\t\t\t\n",       // frame overflow
             "32\tglobal\t1\tkey with space\t\t\n",
             "32\tglobal\t1\t\t\n",             // five columns
             "32\tglobal\t1\t\t\t\textra\n",    // seven columns
             "32\t\t\t\t\thp-5\n",              // item code with a symbol
             "32\t\t\t\t\tabcde\n",             // item code longer than 4
         }) {
        auto bad = header + invalid;
        assert(!D::ValidateAndNormalize(bad, error));
    }
    auto duplicate = header + "32\tglobal\t1\t\t\t\n" + "32\t\t\t\tName\t\n";
    assert(!D::ValidateAndNormalize(duplicate, error));
    auto wrongHeader = std::string("state_id\ticon_atlas\ticon_frame\tname_key\tname\n");
    assert(!D::ValidateAndNormalize(wrongHeader, error));
    auto noFinalLf = header + "32\tglobal\t1\t\t\t";
    assert(!D::ValidateAndNormalize(noFinalLf, error));
    auto longName = header + "32\t\t\t\t" + std::string(64, 'x') + "\t\n";
    assert(!D::ValidateAndNormalize(longName, error));

    // Compiled rows come back sorted; Auto keeps the skill icon.
    const std::vector<D::Row> rows{
        MakeRow(350, "", 0, "", "Custom", "vps"),
        MakeRow(32, "barbarian", 26, "", ""),
    };
    std::vector<D::Definition> definitions;
    assert(D::BuildDefinitions(rows.data(), rows.size(), definitions, error));
    assert(definitions.size() == 2);
    assert(definitions[0].stateId == 32 && definitions[0].iconAtlas == Core::BuffIconAtlas::Barbarian
        && definitions[0].iconFrame == 26 && definitions[0].itemCode.empty());
    assert(definitions[1].stateId == 350 && definitions[1].iconAtlas == Core::BuffIconAtlas::Auto
        && definitions[1].iconFrame == Core::AutoIconFrame && definitions[1].name == "Custom"
        && definitions[1].itemCode == "vps");
    assert(D::BuildDefinitions(nullptr, 0, definitions, error) && definitions.empty());
    assert(!D::BuildDefinitions(nullptr, 1, definitions, error));

    // Registry lookups used by the tracker (icons) and BuffHud (names, item pictures).
    auto& registry = Core::BuffPresentations();
    std::vector<Core::BuffPresentation> presentations(2);
    presentations[0] = {.stateId = 32, .iconAtlas = Core::BuffIconAtlas::Barbarian, .iconFrame = 26};
    presentations[1] = {.stateId = 350, .name = "Custom", .itemCode = "vps"};
    registry.Replace(std::move(presentations));
    std::uint16_t frame{};
    assert(registry.FindIcon(32, atlas, frame) && atlas == Core::BuffIconAtlas::Barbarian && frame == 26);
    assert(!registry.FindIcon(350, atlas, frame));
    assert(!registry.FindIcon(0, atlas, frame));
    char name[16]{};
    assert(registry.FindName(350, name, sizeof(name)) && std::string(name) == "Custom");
    assert(!registry.FindName(32, name, sizeof(name)) && name[0] == '\0');
    char item[8]{};
    assert(registry.FindItemCode(350, item, sizeof(item)) && std::string(item) == "vps");
    assert(!registry.FindItemCode(32, item, sizeof(item)) && item[0] == '\0');
    char tiny[4]{};
    assert(!registry.FindName(350, tiny, sizeof(tiny)));
    registry.Clear();
    assert(registry.Count() == 0 && !registry.FindName(350, name, sizeof(name)));

    std::cout << "PASS: buff-hud-display.txt parsing, validation and lookup\n";
    return 0;
}
