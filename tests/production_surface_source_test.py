from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = (ROOT / "companion/data/global/ui/layouts/buff-panel/BuffHudhd.json").read_text(encoding="utf-8")
sources = "\n".join(p.read_text(encoding="utf-8") for p in sorted((ROOT / "src").rglob("*.cpp")))
sources += "\n" + "\n".join(p.read_text(encoding="utf-8") for p in sorted((ROOT / "src").rglob("*.hpp")))

banned = {
    "StartDebugTest": "fake buff display harness",
    "DebugClock": "synthetic debug clock",
    "DebugKeyBase": "fake buff key namespace",
    "MousePolicy": "mouse A/B policy",
    "buff-panel-tracker": "tracker telemetry command",
    "buff-panel-status": "duplicate status command",
    "rebuild-icons": "manual resolver rebuild command",
    "StatReadInterceptor": "unused stat interception bus",
    "ReadEffective(": "unused intercepted stat-read API",
    "PollQueueFailures": "HUD telemetry counter",
    "RejectedNoExpiry": "dead tracker telemetry counter",
    "Player Buff StatList Probe": "probe provenance string",
    "Skill Icon HUD Probe": "probe provenance string",
}
for token, description in banned.items():
    assert token not in sources, f"production source still contains {description}: {token}"

assert sources.count('RegisterConsoleCommand(\n            "buff-panel"') == 1
assert "ApplyInputIsolation" in sources
assert "UpdateHoverNamePresentation" in sources
assert "RegisterHoverOverlay" in sources
assert "DrawHoverOverlay" in sources
assert "PublishHoverOverlay" in sources
assert "D2RL::OverlayService" in sources
assert "Overlay->drawText" in sources
assert "Overlay->measureText" in sources
assert "std::array<char, HoverTextBytes> hoverText" in sources
assert "disable(slot.countdown)" in sources
assert "disable(slot.slot)" in sources
assert "disable(GridWidget)" in sources
assert "disable(HudPanel)" in sources
assert "const double gridLeft = static_cast<double>(gridRect.x)" in sources
assert "const double gridTop = static_cast<double>(gridRect.y)" in sources
assert "EnsurePanelOpen" in sources
assert "Panels->getPanelInfo" in sources
assert "PresentationState::Open" in sources
assert "set(slot.tooltip, occupied && state.tooltipVisible)" not in sources
assert "StatListBuffMetadataBytes = 0x30" in sources
assert "TooltipReserveBytes = TooltipReserveLength + 1" in sources
assert '"type": "FocusableWidget"' not in LAYOUT
assert '"name": "HoverName"' not in LAYOUT
assert "__BUFF_HUD_HOVER_NAME_RESERVE" not in LAYOUT
assert "__BUFF_HUD_HOVER_NAME_RESERVE" not in sources
assert "WriteHoverNameText" not in sources
assert "qualifiedHoverNameBuffer" not in sources
assert '"fitToParent": true' in LAYOUT
print("PASS: production runtime surface contains no known probe/debug/telemetry remnants")
