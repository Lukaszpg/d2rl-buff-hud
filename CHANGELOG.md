# Buff Panel changelog

## 1.0.11 — production runtime cleanup (2026-09-29)
- Remove display-only fake-buff/test commands and the synthetic debug clock.
- Remove reversible mouse-policy A/B modes and permanently retain the known-good gameplay input-isolation behavior.
- Collapse console diagnostics to `buff-panel [status]`; remove duplicate `buff-panel-status`, `buff-panel-tracker`, manual icon rebuild and display-clear commands.
- Remove HUD/tracker hot-path telemetry counters and last-witness bookkeeping; retain only synchronization/session state and actionable startup/runtime errors.
- Replace the unused stat-read interception bus with a read-only qualified GetUnitStat bridge resolver.
- Stop querying unused Item/Inventory/Interaction/SharedEvent/Diagnostics PluginSDK services and remove unused core diagnostic APIs.
- Keep native byte/range checks, safe RIP-indirect chaining, CTA skill fallback, finite-expiry validation, tooltip bounds fix, skill resolver correction and regression tests unchanged.

## 1.0.10 — centralized timer metadata and native safety tests (2026-09-29)
- Centralize finite timer qualification in a testable `ResolveTimer()` helper: native skill ID wins, configured `skill_id` is fallback, native expiry remains authoritative, and `skillLevel` is absent from the logical timer model.
- Add Shout fallback skill ID 138 alongside Battle Orders 149 and Battle Command 155.
- Fix tooltip UTF-8 native-buffer capacity from 128 bytes to the qualified 126-byte payload plus one terminator (127 bytes); add exact-capacity guard-page coverage.
- Strengthen Skills→SkillDesc runtime layout qualification with expansion-skill witnesses that disambiguate skill IDs from descriptor indexes.
- Add regression tests for CTA timer metadata, tooltip storage bounds/binding, and skill/icon/name resolver ambiguity.
- Preserve 1.0.7/1.0.9 resource packaging, mouse-input behavior, native 0x30 StatList readable-range qualification, and finite-expiry lifecycle. Companion-MPQ work is intentionally deferred.

## 1.0.9 — conservative timer identity fix (2026-09-29)
- Rebased on known-good 1.0.7 runtime behavior; 1.0.8 presence-only timer experiment is removed.
- Preserve the original qualified 0x30 StatList readable-range safety boundary and finite-expiry lifecycle.
- Remove skillLevel from timer qualification and diagnostics.
- Timer skill identity resolves native StatList skill first, then optional `skill_id` from buff-hud.txt.
- Embedded Battle Orders / Battle Command fallbacks are Skills.txt IDs 149 / 155.
- No state-specific or Call To Arms-specific code paths.

## 1.0.0 — standalone catalog release (2026-09-19)
- Expanded embedded/default buff-hud.txt using D2R states/skills/itemstatcost reference tables
- Added Bone/Cyclone Armor resource-state collision guard in the **standalone** tracker only
- Added all catalog-row audit with explicit warnings, disabled auras/passives and in-game test plan
