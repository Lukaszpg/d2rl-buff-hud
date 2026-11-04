#include "buff_display_table.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <system_error>

namespace BuffPanel::Systems::BuffTracker::DisplayTable {
namespace {

struct AtlasName final {
    std::string_view name;
    Core::BuffIconAtlas atlas;
};

constexpr std::array<AtlasName, 9> AtlasNames{{
    {"amazon", Core::BuffIconAtlas::Amazon},
    {"sorceress", Core::BuffIconAtlas::Sorceress},
    {"necromancer", Core::BuffIconAtlas::Necromancer},
    {"paladin", Core::BuffIconAtlas::Paladin},
    {"barbarian", Core::BuffIconAtlas::Barbarian},
    {"druid", Core::BuffIconAtlas::Druid},
    {"assassin", Core::BuffIconAtlas::Assassin},
    {"warlock", Core::BuffIconAtlas::Warlock},
    {"global", Core::BuffIconAtlas::Global},
}};

[[nodiscard]] bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i]))
            != std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view Field(const char* value, std::size_t capacity) noexcept {
    return {value, strnlen(value, capacity)};
}

[[nodiscard]] bool PrintableText(std::string_view value) noexcept {
    return std::all_of(value.begin(), value.end(), [](char ch) noexcept {
        const auto byte = static_cast<unsigned char>(ch);
        return byte >= 0x20 && byte <= 0x7e;
    });
}

[[nodiscard]] bool ReadUnsigned(std::string_view value, std::uint32_t& output) noexcept {
    if (value.empty()) {
        output = 0;
        return true;
    }
    std::uint32_t parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return false;
    output = parsed;
    return true;
}

[[nodiscard]] bool ValidItemCode(std::string_view value) noexcept {
    return value.size() <= 4
        && std::all_of(value.begin(), value.end(), [](char ch) noexcept {
               return std::isalnum(static_cast<unsigned char>(ch)) != 0;
           });
}

template <std::size_t N>
void Copy(std::string_view value, char (&output)[N]) noexcept {
    const auto count = std::min(value.size(), N - 1);
    std::memcpy(output, value.data(), count);
    output[count] = '\0';
}

} // namespace

bool ParseAtlas(std::string_view value, Core::BuffIconAtlas& atlas) noexcept {
    if (value.empty()) {
        atlas = Core::BuffIconAtlas::Auto;
        return true;
    }
    for (const auto& candidate : AtlasNames) {
        if (EqualsIgnoreCase(value, candidate.name)) {
            atlas = candidate.atlas;
            return true;
        }
    }
    return false;
}

bool BuildDefinitions(
    const Row* rows,
    std::size_t rowCount,
    std::vector<Definition>& definitions,
    std::string& error) noexcept {
    definitions.clear();
    if (rowCount != 0 && rows == nullptr) {
        error = "buff-hud-display rows missing";
        return false;
    }
    try {
        definitions.reserve(rowCount);
        for (std::size_t index = 0; index < rowCount; ++index) {
            const auto& row = rows[index];
            const auto where = "buff-hud-display row " + std::to_string(index + 1) + ": ";
            if (row.stateId == 0 || row.stateId > 4095) {
                error = where + "state_id must be 1..4095";
                return false;
            }
            Definition definition{};
            definition.stateId = row.stateId;
            if (!ParseAtlas(Field(row.iconAtlas, sizeof(row.iconAtlas)), definition.iconAtlas)) {
                error = where + "unknown icon_atlas";
                return false;
            }
            if (definition.iconAtlas == Core::BuffIconAtlas::Auto) {
                if (row.iconFrame != 0) {
                    error = where + "icon_frame needs an icon_atlas";
                    return false;
                }
            } else {
                if (row.iconFrame == Core::AutoIconFrame) {
                    error = where + "icon_frame 65535 is reserved";
                    return false;
                }
                definition.iconFrame = row.iconFrame;
            }
            const auto nameKey = Field(row.nameKey, sizeof(row.nameKey));
            const auto name = Field(row.name, sizeof(row.name));
            if (!PrintableText(nameKey) || !PrintableText(name)
                || nameKey.find(' ') != std::string_view::npos) {
                error = where + "name_key/name must be printable ASCII (name_key without spaces)";
                return false;
            }
            const auto itemCode = Field(row.itemCode, sizeof(row.itemCode));
            if (!ValidItemCode(itemCode)) {
                error = where + "item_code must be 1-4 letters or digits";
                return false;
            }
            definition.nameKey.assign(nameKey);
            definition.name.assign(name);
            definition.itemCode.assign(itemCode);
            definitions.push_back(std::move(definition));
        }
        std::sort(definitions.begin(), definitions.end(),
            [](const Definition& lhs, const Definition& rhs) noexcept { return lhs.stateId < rhs.stateId; });
        const auto duplicate = std::adjacent_find(definitions.begin(), definitions.end(),
            [](const Definition& lhs, const Definition& rhs) noexcept { return lhs.stateId == rhs.stateId; });
        if (duplicate != definitions.end()) {
            error = "buff-hud-display: duplicate state_id " + std::to_string(duplicate->stateId);
            definitions.clear();
            return false;
        }
        return true;
    } catch (...) {
        definitions.clear();
        error = "exception while building buff-hud-display definitions";
        return false;
    }
}

bool ValidateAndNormalize(std::string& bytes, std::string& error) noexcept {
    try {
        if (bytes.empty() || bytes.size() > MaximumBytes) {
            error = "buff-hud-display.txt empty or exceeds 1 MiB";
            return false;
        }
        std::string normalized;
        normalized.reserve(bytes.size());
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            const auto character = static_cast<unsigned char>(bytes[index]);
            if (character == '\r') {
                if (index + 1 >= bytes.size() || bytes[index + 1] != '\n') {
                    error = "buff-hud-display.txt: bare CR; use LF or CRLF line endings";
                    return false;
                }
                continue;
            }
            if (character != '\n' && character != '\t' && (character < 0x20 || character > 0x7e)) {
                error = "buff-hud-display.txt: non-printable/non-ASCII byte; save as plain ASCII without BOM";
                return false;
            }
            normalized.push_back(static_cast<char>(character));
        }
        if (normalized.back() != '\n') {
            error = "buff-hud-display.txt requires a final line feed";
            return false;
        }
        const auto headerEnd = normalized.find('\n');
        if (std::string_view(normalized).substr(0, headerEnd) != Header) {
            error = "buff-hud-display.txt header must exactly match its six columns";
            return false;
        }

        std::vector<Row> rows;
        std::size_t lineStart = headerEnd + 1;
        while (lineStart < normalized.size()) {
            const auto lineEnd = normalized.find('\n', lineStart);
            const std::string_view line(normalized.data() + lineStart, lineEnd - lineStart);
            lineStart = lineEnd + 1;
            if (line.empty()) continue;
            const auto where = "buff-hud-display.txt row " + std::to_string(rows.size() + 2) + ": ";

            std::array<std::string_view, ColumnCount> cells{};
            std::size_t cellStart{};
            for (std::size_t column = 0; column + 1 < ColumnCount; ++column) {
                const auto separator = line.find('\t', cellStart);
                if (separator == std::string_view::npos) {
                    error = where + "fewer than six columns";
                    return false;
                }
                cells[column] = line.substr(cellStart, separator - cellStart);
                cellStart = separator + 1;
            }
            cells[ColumnCount - 1] = line.substr(cellStart);
            if (cells[ColumnCount - 1].find('\t') != std::string_view::npos) {
                error = where + "more than six columns";
                return false;
            }

            Row row{};
            std::uint32_t frame{};
            if (!ReadUnsigned(cells[0], row.stateId) || !ReadUnsigned(cells[2], frame) || frame > 0xFFFFu) {
                error = where + "invalid unsigned decimal field";
                return false;
            }
            if (cells[1].size() >= sizeof(row.iconAtlas)
                || cells[3].size() >= sizeof(row.nameKey)
                || cells[4].size() >= sizeof(row.name)
                || cells[5].size() >= sizeof(row.itemCode)) {
                error = where + "icon_atlas/name_key/name/item_code too long";
                return false;
            }
            row.iconFrame = static_cast<std::uint16_t>(frame);
            Copy(cells[1], row.iconAtlas);
            Copy(cells[3], row.nameKey);
            Copy(cells[4], row.name);
            Copy(cells[5], row.itemCode);
            rows.push_back(row);
        }

        std::vector<Definition> definitions;
        if (!BuildDefinitions(rows.data(), rows.size(), definitions, error)) return false;
        bytes.swap(normalized);
        return true;
    } catch (...) {
        error = "exception while validating buff-hud-display.txt";
        return false;
    }
}

} // namespace BuffPanel::Systems::BuffTracker::DisplayTable
