#pragma once

#include "core/buff_display_bus.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace BuffPanel::Systems::BuffTracker::DisplayTable {

// Optional per-state presentation overrides, kept out of buff-hud.txt so that
// existing buff-hud.txt overrides stay valid. The companion ships a header-only
// default; a row only changes what its state looks like, never what is tracked.
inline constexpr std::string_view VirtualPath =
    "data/global/excel/d2rloader/buff-panel/buff-hud-display.txt";
inline constexpr std::string_view Header =
    "state_id\ticon_atlas\ticon_frame\tname_key\tname\titem_code";
inline constexpr std::size_t ColumnCount = 6;
inline constexpr std::size_t MaximumBytes = 1024u * 1024u;

// Compiled custom-table row. Ascii lengths include the trailing NUL.
struct Row final {
    std::uint32_t stateId{};
    char iconAtlas[16]{};
    std::uint16_t iconFrame{};
    char nameKey[64]{};
    char name[64]{};
    char itemCode[8]{};
};

struct Definition final {
    std::uint32_t stateId{};
    // Auto keeps the skill-derived icon; otherwise iconFrame is a frame of that atlas.
    Core::BuffIconAtlas iconAtlas{Core::BuffIconAtlas::Auto};
    std::uint16_t iconFrame{Core::AutoIconFrame};
    std::string nameKey{};
    std::string name{};
    // Item whose inventory picture replaces the icon when the layout has an
    // Item_<code> widget in the slot; empty keeps the atlas icon.
    std::string itemCode{};
};

// "" -> Auto. Names match the atlas widgets of the layout, without case.
[[nodiscard]] bool ParseAtlas(std::string_view value, Core::BuffIconAtlas& atlas) noexcept;

// Validates compiled rows and returns them sorted by state_id. Duplicate
// state_id rows, unknown atlases, a frame without an atlas, the reserved frame
// 0xFFFF, names with control characters and item codes that are not 1-4
// letters or digits are rejected.
[[nodiscard]] bool BuildDefinitions(
    const Row* rows,
    std::size_t rowCount,
    std::vector<Definition>& definitions,
    std::string& error) noexcept;

// Strict TSV check of the source text, matching what D2RLoader's custom table
// compiler accepts (ASCII, exact header, final LF; CRLF is normalized).
[[nodiscard]] bool ValidateAndNormalize(std::string& bytes, std::string& error) noexcept;

} // namespace BuffPanel::Systems::BuffTracker::DisplayTable
