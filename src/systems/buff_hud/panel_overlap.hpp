#pragma once

namespace BuffPanel::Systems::BuffHud::Internal {

// Where D2R's side panels sit on its HD UI screen (2160 units high), for
// hide_with_panels. The game's panel widgets do not report their resolved
// rects to plugins, so the boxes are measured from screenshots (D2R 93847): a
// left panel (character, quests, waypoints, a vendor) ends 1392 units from the
// left edge, a right panel (inventory, skill tree) starts 1392 units from the
// right edge; both end 1716 units from the top. The icons' lower rows sit below
// them.
enum class PanelSide { Left, Right };

struct UiBox final {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

inline constexpr double UiScreenHeight = 2160.0;
inline constexpr double SidePanelWidth = 1392.0;
inline constexpr double SidePanelBottom = 1716.0;

[[nodiscard]] constexpr UiBox SidePanelBox(PanelSide side, double screenWidth) noexcept {
    return side == PanelSide::Left
        ? UiBox{0.0, 0.0, SidePanelWidth, SidePanelBottom}
        : UiBox{screenWidth - SidePanelWidth, 0.0, screenWidth, SidePanelBottom};
}

[[nodiscard]] constexpr bool Overlaps(const UiBox& a, const UiBox& b) noexcept {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

} // namespace BuffPanel::Systems::BuffHud::Internal
