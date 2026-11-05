#pragma once

#include <string>

namespace BuffPanel::Core {

// Removes D2's colour codes from localized text: "ÿc" and the code character
// after it ("ÿc1Battle Command" -> "Battle Command"). D2R's string tables
// give "ÿc" as U+E07E (UTF-8 EE 81 BE); "ÿ" as UTF-8 (C3 BF) or a lone 0xFF
// byte followed by 'c' counts too.
inline void StripColourCodes(std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        std::size_t marker = 0;
        if (text.compare(i, 3, "\xEE\x81\xBE") == 0) {
            marker = 3;
        } else if (text.compare(i, 3, "\xC3\xBF" "c") == 0) {
            marker = 3;
        } else if (text.compare(i, 2, "\xFF" "c") == 0) {
            marker = 2;
        }
        if (marker != 0) {
            i += marker;
            // The code is one ASCII character; a multi-byte one is left to show.
            if (i < text.size() && static_cast<unsigned char>(text[i]) < 0x80) ++i;
            continue;
        }
        out.push_back(text[i++]);
    }
    text.swap(out);
}

} // namespace BuffPanel::Core
