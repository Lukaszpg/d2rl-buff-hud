#pragma once

namespace BuffPanel::Systems::BuffHud::Internal {

// Where D2R's side panels sit on its UI screen, for hide_with_panels. The
// game's panel widgets do not report their resolved rects to plugins, so the
// boxes are measured from screenshots (D2R 93847):
// - HD (UI space 2160 high): a left panel (character, quests, waypoints, a
//   vendor) ends 1392 units from the left edge, a right panel (inventory,
//   skill tree) starts 1392 units from the right edge; both end 1716 units from
//   the top. The icons' lower rows sit below them.
// - Legacy: the classic panels are 320 units wide on an 800-wide screen; on a
//   wider classic screen (a widescreen classic plugin) they are 400 wide. They
//   reach the control panel, so the icons are always level with them.
enum class PanelSide { Left, Right };

struct UiBox final {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

inline constexpr double UiScreenHeight = 2160.0;
inline constexpr double HdSidePanelWidth = 1392.0;
inline constexpr double HdSidePanelBottom = 1716.0;
inline constexpr double ClassicSidePanelWidth = 320.0;
inline constexpr double WideClassicSidePanelWidth = 400.0;
inline constexpr double ClassicScreenWidth = 800.0;

[[nodiscard]] constexpr UiBox SidePanelBox(
    PanelSide side, double screenWidth, double screenHeight, bool hd) noexcept {
    const double width = hd ? HdSidePanelWidth
        : screenWidth > ClassicScreenWidth ? WideClassicSidePanelWidth : ClassicSidePanelWidth;
    const double bottom = hd ? HdSidePanelBottom : screenHeight;
    return side == PanelSide::Left
        ? UiBox{0.0, 0.0, width, bottom}
        : UiBox{screenWidth - width, 0.0, screenWidth, bottom};
}

[[nodiscard]] constexpr bool Overlaps(const UiBox& a, const UiBox& b) noexcept {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

} // namespace BuffPanel::Systems::BuffHud::Internal
