# Buff HUD — D2RLoader plugin

A standalone 3×7 temporary-buff HUD and countdown tracker.

## Acknowledgements

Many thanks to [RuffnecKk](https://github.com/RuffDood) for being a great teacher and awesome dude.
Make sure to check out his amazing plugin [suite](https://github.com/RuffDood/RuffnecKk-D2RLoader-Suite)!

## What it provides

- 21 reusable display-only buff slots in a 3×7, lower-left-fill panel.
- Timed entries with countdowns based on the game's authoritative 25-frame-per-second clock.
- Resource entries with an optional current/max stat counter (e.g. Bone Armor), independent of time remaining.
- Automatic icon and localized name discovery using game Skills/SkillDesc and original skill icon atlases (Amazon through Warlock and Global).
- Table-driven whitelist from `buff-hud.txt`: `name`, `state_id`, `display_type`, `value_stat`, `max_stat`, `skill_id`, `value_shift`, `enabled`.
- Excludes curses and shared-stash proxy states; built-in semantic checks on native buff metadata.

### Default table

Timed self-skill, shrine, temporary potion and resource candidates are included; auras/permanent passives are recorded with `enabled=0` because they are **not** finite countdowns. A row's presence does **not** guarantee that the game emits the metadata necessary for a HUD countdown. Rows without native skill attribution need a configured `skill_id` to supply their icon/name. Battle Orders, Battle Command and Shout include this mapping, including when cast from Call to Arms. Absorb pools have independent resource presentation and state disambiguation.

`buff-hud.txt` with tabs:

```text
name\tstate_id\tdisplay_type\tvalue_stat\tmax_stat\tskill_id\tvalue_shift\tenabled
fade\t159\ttimer\t0\t0\t0\t0\t1
bone_armor\t14\tresource\t132\t133\t68\t8\t1
```

The `\t` escapes above indicate tab separators; use actual tabs in the text file. For timer entries, `skill_id=0` uses native attribution; a nonzero ID is a fallback when the native skill is missing. The live state must still provide a valid expiration frame. Skill level is not required. `value_stat`, `max_stat`, and `value_shift` stay zero for timers. For resource entries, `value_stat` and `max_stat` are live stat IDs, `skill_id` supplies the icon/name, and `value_shift` scales the displayed value.

This fork's bundled resource mapping targets Reimagined: Psychic Ward uses stats **427/428**. Stock IDs 361/362 are corruption flags in Reimagined. Other mods may need different mappings. Resource entries require their attached native state as well as positive pool values; unrelated item stats cannot activate a buff entry.

Custom buffs are supported through the same table: add the mod's state ID and an optional fallback skill ID, enable the row, and rebuild the companion MPQ. For example, a mod with state 350 and skill 510 could add `custom_buff\t350\ttimer\t0\t0\t510\t0\t1` (replace `\t` with tabs and use the actual mod IDs). Icons/names resolve from the active mod's Skills/SkillDesc tables. Unlisted states, curses, permanent effects, and states without a finite expiration are not automatically displayed. No mod MPQ edits are required.

## Installation

Build this fork or use its `dist/` output. Copy **both** `d2rl-buff-panel.dll` and `d2rl-buff-panel.mpq` into one plugin scope:

```text
<game>/mods/ReimaginedLadder/d2rloader/plugins/
<game>/mods/<mod-name>/d2rloader/plugins/
<game>/d2rloader/plugins/
```

For the ladder bundle, place the pair in its mod-scoped `d2rloader/plugins/` directory. No files need to be copied into `Reimagined.mpq` or `ReimaginedLadder.mpq`. The original project's DLL-only/data.zip instructions do not apply to this fork. See [INSTALL.md](INSTALL.md) for the recipient instructions included in `dist/`.

The companion contains exactly the plugin-owned resources:

```text
data/global/ui/layouts/buff-panel/BuffHudhd.json
data/global/excel/d2rloader/buff-panel/buff-hud.txt
```

D2RLoader resolves these from the companion. Its normal active-mod overrides still take priority, including packed mod MPQs. The DLL does not register an embedded resource that could hide edits to the companion. D2RLoader compiles the custom TXT table during table loading; this package does not contain precompiled BINs and needs no `-txt` flag.

## Building

Requirements: Windows x64, MSVC with the Windows SDK, CMake 3.29+, Ninja for the presets, and [D2RLCompiler](https://github.com/D2RLoader). The SDK is a pinned submodule: **0.3.0, ABI 4**, upstream revision `717f727a0ec52912d1558764345f8fa3453a2bd6` (latest upstream HEAD checked September 29, 2026).

```powershell
git submodule update --init --recursive
.\scripts\build.ps1 -Preset release -Dist
```

The script imports the installed Visual Studio x64 toolchain, builds, runs the tests, and publishes `dist/`. Set the compiler path once if it is not in the adjacent `../d2rl-compiler/` folder:

```powershell
cmake --preset release -DD2RL_COMPILER=C:/tools/D2RLCompiler.exe
```

An existing workspace can supply `D2RLPlugin::D2RLPlugin`. Otherwise CMake uses `external/PluginSDK`, `BUFF_PANEL_SDK_DIR`, or an installed `D2RLPlugin` package (minimum 0.3.0). For an installed package, set `BUFF_PANEL_SDK_LICENSE_FILE` to its license for redistribution.

From an x64 MSVC developer shell, a Visual Studio generator also works:

```powershell
cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64 -DD2RL_COMPILER=C:/tools/D2RLCompiler.exe
cmake --build build/vs --config Release --parallel
ctest --test-dir build/vs -C Release --output-on-failure
cmake --build build/vs --config Release --target dist
```

The `buff_panel` target builds the DLL. The explicit `buff_panel_companion` target validates and packs the MPQ beside it under `build/<preset>/stage/Release/d2rloader/plugins/`; `dist` depends on both. This separation lets ordinary CI compile/test the DLL without requiring the private packer, while release builds still fail closed if `D2RLCompiler.exe` is unavailable or companion resources are invalid. Like MoveOnly's resource packaging, the companion target uses `D2RLCompiler pack`, which requires no per-user linked game installation. Debug builds cannot publish `dist`.

`dist/` includes the DLL/MPQ pair, SHA-256 checksums, installation instructions, and both license notices. It is a relative install overlay: copy its `d2rloader` folder beneath the chosen game/mod root. The build does not install into the game or publish a ladder bundle.

Optional archive verification (Python plus `mpyq==0.2.5`): run `python tests/verify_package.py dist/d2rloader/plugins/d2rl-buff-panel.mpq`. This checks that only the two expected payloads and the MPQ listfile are present, and compares both payloads byte-for-byte with `companion/`.

**Compatibility:** native addresses and byte contracts remain qualified only for D2R build **93847**. The SDK upgrade does not qualify other game builds; existing native guards remain enabled.

## Customizing the panel and whitelist

Edit the files under `companion/data/` and rebuild the plugin target to repack the MPQ. Asset-only edits do not require C++ changes.

- Layout: `companion/data/global/ui/layouts/buff-panel/BuffHudhd.json`.
- Buff selection: `companion/data/global/excel/d2rloader/buff-panel/buff-hud.txt`.

Change `BuffGrid.fields.rect.x` and `.y` to move the whole panel. The default is `x: -150, y: -515`; increasing x moves right, and making y more negative moves up. Keep the outer anchor and the 21 child slots unchanged for a simple reposition. There is no drag-to-move UI.

Install the rebuilt MPQ next to the matching DLL and restart D2R. Any pre-existing mod override at the same virtual path takes priority, so inspect that if your companion changes do not appear. Use `buff-panel status` for the compact production status line, then verify real buffs separately.

## Commands

```text
buff-panel status
buff-panel mouse status
buff-panel mouse gameplay
buff-panel mouse no-tooltips
buff-panel mouse original
buff-panel test 15 3
buff-panel clear
buff-panel rebuild-icons
buff-panel-tracker
buff-panel-status
```

Test while a configured buff is **currently active**. `buff-panel-tracker` reports whether the whitelist loaded, whether a timed state was discovered, and whether its live expiry was published; `buff-panel status` reports currently displayed entries and the HUD frame clock.

Version 1.0.11 reconciles the companion-MPQ packaging work with the production-clean runtime. The panel uses fixed gameplay input isolation: atlas buttons, slot focus surfaces, the grid, and the panel are kept disabled for hit testing while rendering remains active. Development-only mouse A/B modes, fake buffs, synthetic clocks, and probe telemetry are not part of the production DLL.

## Known limitations / verification

- It is not a generic arbitrary-D2R-build plugin: qualified low-level offsets and bridge signatures are used and fail closed on mismatch.
- `buff-panel` owns native patching; do not run it alongside another uncoordinated plugin that changes the same bridge without safe chaining.

## Source provenance

`src/systems/buff_hud/`, `src/systems/buff_tracker/`, the relevant shared display/stat buses, the native qualification constants. The copy was renamed and isolated under `BuffPanel::`, given its own plugin entry point/CMake/resource paths/console commands, and the *standalone-only* read-only getter resolver was added.

## Distributing

Distribute the matching DLL and MPQ together with the supported-game-build information, installation instructions, and included license notices. Preserve upstream attribution. The PluginSDK submodule has its own license.


## GitHub Actions

`CI` runs on pushes to `main`, pull requests, and manual dispatch. It initializes the PluginSDK submodule, verifies the pinned D2RLCompiler, runs the Python source checks, packs and verifies the companion MPQ, builds the Release DLL and regression tests, runs CTest, and uploads the DLL/MPQ pair as the CI artifact.

`Release` is a manual workflow on `main`, following the same bump/tag pattern as UnHoarder. Configure these repository secrets before running it:

The approved compiler is pinned in the repository at `external/D2RLCompiler/D2RLCompiler.exe`; no Actions secret or network download is required. CI and Release verify its SHA-256 before using it.

Release increments `VERSION`, synchronizes all compiled version metadata, packs and verifies the companion MPQ first, builds/tests the DLL second, commits and tags the successful version, and publishes `d2rl-buff-panel.dll`, `d2rl-buff-panel.mpq`, a source ZIP, and `SHA256SUMS.txt`.
