#include "core/display_text.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>

using BuffPanel::Core::StripColourCodes;

namespace {
std::string Stripped(std::string text) {
    StripColourCodes(text);
    return text;
}
} // namespace

int main() {
    assert(Stripped("\xC3\xBF" "c1Battle Command") == "Battle Command");
    assert(Stripped("\xEE\x81\xBE" "1Battle Command") == "Battle Command");
    assert(Stripped("\xEE\x81\xBE") == "");
    assert(Stripped("\xFF" "c4Gold") == "Gold");
    assert(Stripped("Holy \xC3\xBF" "c3Armor\xC3\xBF" "c0") == "Holy Armor");
    assert(Stripped("Battle Orders") == "Battle Orders");
    assert(Stripped("\xC3\xBF") == "\xC3\xBF");
    assert(Stripped("\xC3\xBF" "c") == "");
    assert(Stripped("Caf\xC3\xA9") == "Caf\xC3\xA9");
    std::cout << "PASS: colour codes stripped from display text\n";
    return 0;
}
