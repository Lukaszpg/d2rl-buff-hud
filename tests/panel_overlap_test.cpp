#include "systems/buff_hud/panel_overlap.hpp"

#include <cstdio>

using namespace BuffPanel::Systems::BuffHud::Internal;

namespace {

// Slot n of the bottom row of BuffHud.json (legacy: grid x -182, y -264 under
// the bottom centre, 48 px slots 52 apart, row at y 128) on a screen w x 600.
UiBox LegacySlot(int n, double w) {
    const double left = w * 0.5 - 182.0 + 52.0 * n;
    const double top = 600.0 - 264.0 + 128.0;
    return {left, top, left + 48.0, top + 48.0};
}

// The same for BuffHudhd.json (grid x -150, y -515, 96 px slots 112 apart;
// rows at y 210, 105, 0) on a UI screen w x 2160.
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
    const auto left = SidePanelBox(PanelSide::Left, 3840, UiScreenHeight, true);
    const auto right = SidePanelBox(PanelSide::Right, 3840, UiScreenHeight, true);
    check(!Overlaps(HdSlot(6, 0, 3840), right), "bottom row clear of the right panel");
    check(!Overlaps(HdSlot(6, 1, 3840), right), "middle row clear of the right panel");
    check(Overlaps(HdSlot(6, 2, 3840), right), "top row's seventh slot under the right panel");
    check(!Overlaps(HdSlot(5, 2, 3840), right), "top row's sixth slot clear of the right panel");
    check(!Overlaps(HdSlot(0, 2, 3840), left), "left panel clear of the grid");
    // Legacy on a 16:9 classic screen (1066 x 600): a left panel covers the
    // first slot, a right panel the seventh only.
    const auto left169 = SidePanelBox(PanelSide::Left, 1066, 600, false);
    const auto right169 = SidePanelBox(PanelSide::Right, 1066, 600, false);
    check(Overlaps(LegacySlot(0, 1066), left169), "legacy 16:9 left panel over slot 1");
    check(!Overlaps(LegacySlot(1, 1066), left169), "legacy 16:9 left panel clear of slot 2");
    check(!Overlaps(LegacySlot(5, 1066), right169), "legacy 16:9 right panel clear of slot 6");
    check(Overlaps(LegacySlot(6, 1066), right169), "legacy 16:9 right panel over slot 7");
    // Legacy 16:10 (960 x 600): the right panel reaches the sixth slot.
    const auto right1610 = SidePanelBox(PanelSide::Right, 960, 600, false);
    check(!Overlaps(LegacySlot(4, 960), right1610), "legacy 16:10 right panel clear of slot 5");
    check(Overlaps(LegacySlot(5, 960), right1610), "legacy 16:10 right panel over slot 6");
    // Classic 4:3 (800 x 600): 320-wide panels cover both ends.
    check(Overlaps(LegacySlot(0, 800), SidePanelBox(PanelSide::Left, 800, 600, false)), "4:3 left panel");
    check(Overlaps(LegacySlot(6, 800), SidePanelBox(PanelSide::Right, 800, 600, false)), "4:3 right panel");
    check(!Overlaps(UiBox{0, 0, 10, 10}, UiBox{10, 0, 20, 10}), "touching edges do not overlap");
    if (failures == 0) std::printf("PASS: panel overlap\n");
    return failures == 0 ? 0 : 1;
}
