#include "systems/buff_hud/panel_overlap.hpp"

#include <cstdio>

using namespace BuffPanel::Systems::BuffHud::Internal;

namespace {

// Slot n of a row of BuffHudhd.json (grid x -150, y -515 under the bottom
// centre, 96 px slots 112 apart; rows at y 210, 105, 0) on a UI screen w x 2160.
UiBox HdSlot(int n, int row, double w) {
    const double left = w * 0.5 - 150.0 + 112.0 * n;
    const double top = UiScreenHeight - 515.0 + 210.0 - 105.0 * row;
    return {left, top, left + 96.0, top + 96.0};
}

} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* what) {
        if (!condition) {
            std::printf("FAIL: %s\n", what);
            ++failures;
        }
    };
    // 16:9 (3840 x 2160): the two lower rows are below the panels, the top
    // row's seventh slot reaches the right panel, the left panel none.
    const auto left = SidePanelBox(PanelSide::Left, 3840);
    const auto right = SidePanelBox(PanelSide::Right, 3840);
    check(!Overlaps(HdSlot(6, 0, 3840), right), "bottom row clear of the right panel");
    check(!Overlaps(HdSlot(6, 1, 3840), right), "middle row clear of the right panel");
    check(Overlaps(HdSlot(6, 2, 3840), right), "top row's seventh slot under the right panel");
    check(!Overlaps(HdSlot(5, 2, 3840), right), "top row's sixth slot clear of the right panel");
    check(!Overlaps(HdSlot(0, 2, 3840), left), "left panel clear of the grid");
    check(!Overlaps(UiBox{0, 0, 10, 10}, UiBox{10, 0, 20, 10}), "touching edges do not overlap");
    if (failures == 0) std::printf("PASS: panel overlap\n");
    return failures == 0 ? 0 : 1;
}
