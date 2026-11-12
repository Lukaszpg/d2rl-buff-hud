#pragma once

#include <cstring>

namespace BuffPanel::Core {

// buff-panel.toml is small: [buff_panel] hide_with_panels = true|false. A
// missing key, or a value that is neither true nor false, keeps the default.
// Only lines inside [buff_panel] count; # starts a comment.
struct PluginConfig final {
    // Hide the icons while the stash, the cube or a side panel over them is open.
    bool hideWithPanels{true};
};

[[nodiscard]] inline PluginConfig ParsePluginConfig(const char* text) noexcept {
    PluginConfig config{};
    if (text == nullptr) return config;
    bool inSection = false;
    for (const char* line = text; *line != '\0';) {
        const char* end = line;
        while (*end != '\0' && *end != '\n') ++end;
        const char* s = line;
        while (s < end && (*s == ' ' || *s == '\t')) ++s;
        const char* e = s;
        while (e < end && *e != '#' && *e != '\r') ++e;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
        const auto length = static_cast<std::size_t>(e - s);
        if (length > 0 && *s == '[') {
            inSection = length == 12 && std::strncmp(s, "[buff_panel]", 12) == 0;
        } else if (inSection && length > 0) {
            const char* eq = s;
            while (eq < e && *eq != '=') ++eq;
            if (eq < e) {
                const char* keyEnd = eq;
                while (keyEnd > s && (keyEnd[-1] == ' ' || keyEnd[-1] == '\t')) --keyEnd;
                const char* value = eq + 1;
                while (value < e && (*value == ' ' || *value == '\t')) ++value;
                const auto keyLength = static_cast<std::size_t>(keyEnd - s);
                const auto valueLength = static_cast<std::size_t>(e - value);
                if (keyLength == 16 && std::strncmp(s, "hide_with_panels", 16) == 0) {
                    if (valueLength == 4 && std::strncmp(value, "true", 4) == 0) config.hideWithPanels = true;
                    if (valueLength == 5 && std::strncmp(value, "false", 5) == 0) config.hideWithPanels = false;
                }
            }
        }
        line = *end == '\n' ? end + 1 : end;
    }
    return config;
}

} // namespace BuffPanel::Core
