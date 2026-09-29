# Buff Panel — D2RLoader plugin

**1.0.11 production cleanup (2026-09-29):** removes development-only display tests, the synthetic debug clock, mouse-policy A/B modes, tracker/HUD telemetry counters, duplicate diagnostic commands, unused PluginSDK services, and the unused stat-read interception layer. The proven gameplay input-isolation behavior is now fixed rather than switchable. Native fingerprints, readable-range checks, hook chaining, finite-expiry validation, CTA skill fallbacks, tooltip bounds protection, and regression tests remain.

Buff Panel is a standalone 3×7 temporary-buff panel and countdown tracker for D2R build **93847**.

## What it provides

- 21 reusable display-only buff slots in a lower-left-fill 3×7 panel.
- Timed entries based on D2R's authoritative 25-frame-per-second game clock.
- Resource entries with current/max counters such as Bone Armor.
- Runtime Skills→SkillDesc icon resolution using the active compiled data tables.
- Localized skill-name resolution with bounded native tooltip storage.
- A table-driven whitelist from `buff-hud.txt`.
- Fail-closed native byte/range qualification and shared-stash/curse exclusions.
- Fixed gameplay input isolation: Buff Panel widgets are never made interactive.

## Timer metadata

`buff-hud.txt` uses these tab-separated columns:

```text
name	state_id	display_type	value_stat	max_stat	skill_id	value_shift	enabled
```

There is no skill-level field.

For `display_type=timer`:

- `value_stat`, `max_stat`, and `value_shift` must be `0`.
- Native StatList `skill` is preferred when nonzero.
- Configured `skill_id` is a fallback only when native `skill` is zero.
- A finite future native expiry is always required; Buff Panel does not invent durations.

The embedded defaults include:

```text
shout	26	timer	0	0	138	0	1
battle_orders	32	timer	0	0	149	0	1
battle_command	51	timer	0	0	155	0	1
```

For `display_type=resource`, `value_stat`, `max_stat`, and `skill_id` are required; `value_shift` scales the raw values.

## Installation

Copy `d2rl-buff-panel.dll` to one D2RLoader plugin scope:

```text
<Diablo II Resurrected>/d2rloader/plugins/d2rl-buff-panel.dll
```

or:

```text
<Diablo II Resurrected>/mods/<mod-name>/d2rloader/plugins/d2rl-buff-panel.dll
```

The default layout and buff catalog are embedded in the DLL. Loose files are **optional overrides**, not required runtime dependencies.

To override them for one mod, place them under the active mod data root:

```text
data/global/ui/layouts/buff-panel/BuffHudhd.json
data/global/excel/d2rloader/buff-panel/buff-hud.txt
```

Restart D2R after changing either file.

## Moving Buff Panel

Override `BuffHudhd.json` and edit only `BuffGrid.fields.rect.x` / `y` to move the whole panel. The default grid is:

```json
"rect": {
  "x": 50,
  "y": -515,
  "width": 780,
  "height": 300
}
```

Increase `x` to move right; decrease it to move left. More-negative `y` moves upward; less-negative `y` moves downward. Leave the outer anchor and individual slot rectangles unchanged for ordinary repositioning.

## Console command

Production exposes one compact status surface:

```text
buff-panel
buff-panel status
```

It reports current display/session state, panel registration, active layout source, skill resolver readiness, and fixed input isolation. Development commands such as fake buff tests, display clearing, mouse-policy switching, manual resolver rebuilds, and tracker telemetry are intentionally not part of the production DLL.

## Building

### Requirements

- Windows x64 with MSVC/C++20 and Windows SDK.
- CMake 3.29 or newer.
- D2RLoader PluginSDK 0.2.x, plugin ABI 4.
- D2RLoader runtime providing the Resource, Panel, Widget, Thread, Lifecycle, CustomTable, DataTable and Localization services used by the plugin.

The native addresses and byte contracts are qualified for D2R build **93847** only.

### Standalone build

Place the official PluginSDK at `third_party/PluginSDK`, then run from the Buff Panel project root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target buff_panel --parallel
```

An installed `D2RLPlugin` CMake package may be used instead. If Buff Panel is included from a parent workspace that already defines `D2RLPlugin::D2RLPlugin` or `D2RLPluginV4::D2RLPluginV4`, the existing target is reused.

### Regression tests

Tests are disabled by default and are not part of the production DLL. Enable them explicitly:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUFF_PANEL_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Coverage includes CTA timer fallback/expiry semantics, Skills→SkillDesc ambiguity, tooltip exact-capacity storage/binding, and loose override validation.

## Runtime behavior and safety

Buff Panel intentionally retains the native safety mechanisms required by its runtime work:

- executable-build byte fingerprints;
- loader RIP-indirect bridge qualification;
- readable/writable memory-range checks;
- exact-state StatList lookup qualification;
- finite future expiry validation;
- curse/shared-stash exclusion;
- bounded tooltip backing-buffer writes.

These are production safety boundaries, not probe remnants, and should not be bypassed when updating the plugin for another game/loader build.

## Known limitation

The fixed production input-isolation policy disables HUD focus surfaces so the overlay cannot consume world clicks. Consequently native hover interaction is not enabled by a debug/A-B mode in production. The backing tooltip path remains bounded and tested for future presentation work.

## Acknowledgements

Many thanks to RuffnecKk and the D2RLoader/PluginSDK project for the loader infrastructure and guidance that made the standalone plugin possible.

## Distribution

Ship the DLL with matching source/version information and the supported D2R build. PluginSDK is a separate dependency with its own license and is not bundled in this source archive.
