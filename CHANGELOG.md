# Buff HUD changelog

## 1.0.11 — companion-MPQ reconciliation and production cleanup (2026-09-29)
- Reconcile PR #1's companion MPQ/build/distribution architecture with the production runtime from main.
- Keep native-skill-first timer resolution with configured Shout/Battle Orders/Battle Command fallbacks and finite native expiry.
- Keep the 127-byte tooltip backing-buffer fix and corrected Skills->SkillDesc resolver with regression coverage.
- Remove fake buff/debug-clock controls, mouse A/B modes, duplicate diagnostic commands, hot-path telemetry, unused stat interception, and unused SDK services.
- Preserve fixed gameplay input isolation and native fail-closed safety contracts.

## 1.0.10 — upstream integration (2026-09-29)
- Merge upstream 0996a88; retain PluginSDK 0.3.0, companion MPQ packaging, Reimagined mappings, and existing panel position.
- Remove unused skill-level reads while preserving native timer range checks and Call to Arms fallback identity.
- Add upstream mouse modes and diagnostics; preserve native hover as this fork's default.
- Import tooltip null handling and portable buffer tests; verify mouse modes restore parent and child enabled states.

## 1.0.9 — Call to Arms timers and tooltips (2026-09-29)
- Support timer skill-ID fallbacks for native states missing caster metadata, including custom buffs.
- Map Battle Orders, Battle Command and Shout; preserve actual native expiration and recast tracking.
- Fix tooltip qualification and writes exceeding the native string allocation by one byte.
- Hide unavailable tooltips and add live-metadata, custom-icon, schema and guard-page regression coverage.

## 1.0.8 — Reimagined buff compatibility (2026-09-29)
- Disambiguate skill IDs from descriptor indexes using expansion-skill witnesses, restoring icon and localized-name resolution.
- Require an attached state for every resource buff, preventing unrelated positive stats from creating phantom entries.
- Map Psychic Ward to Reimagined stats 427/428 instead of corruption flags 361/362.
- Add resolver regression coverage for colliding IDs, reordered descriptors, real ambiguity, and changed witness data.

## 1.0.7 — companion package (2026-09-29)
- Update to PluginSDK 0.3.0, pinned at upstream revision 717f727a0ec52912d1558764345f8fa3453a2bd6; retain ABI 4 and native build guards.
- Ship the layout and buff whitelist in a companion MPQ beside the DLL; no mod-side files required.
- Resolve resources through D2RLoader and remove duplicate embedded defaults; preserve active-mod override priority.
- Add reproducible builds, asset validation, regression tests, release staging, checksums, and redistribution notices.

## 1.0.0 — standalone catalog release (2026-09-19)
- Expanded embedded/default buff-hud.txt using D2R states/skills/itemstatcost reference tables
- Added Bone/Cyclone Armor resource-state collision guard in the **standalone** tracker only
- Added all catalog-row audit with explicit warnings, disabled auras/passives and in-game test plan
