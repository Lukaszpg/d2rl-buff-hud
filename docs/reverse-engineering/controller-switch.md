# Controller / keyboard-mouse HUD switching

## Symptom and cause

Switching input mode could expose all 21 slots, default atlas icons and the
`__BUFF_PANEL_TIMER_RESERVE_00__` backing-text marker instead of the current buffs.
The marker matches the companion layout, not a localization lookup failure.

The renderer caches slot visibility, assignment and timer/resource values.
`HideSlot` previously skipped slots cached as empty, and active-slot rendering
skipped unchanged assignments/values. Those shortcuts assume native presentation
has not been reset or reconstructed. The PluginSDK 0.3.0 widget contract explicitly
says handles are paths resolved on every call, so a valid SDK handle does not
establish that the current native widget still has the cached presentation.

The exact game-side reconstruction sequence has not been traced. This explanation
is supported by source inspection, the exposed marker and the live repair result.

## Repair and native contract

Each UI poll compares current countdown widget/backing-buffer identities and
checks for a restored reserve marker on previously displayed timers. A change
invalidates that slot's rendered state and reapplies existing input isolation.
Empty-slot visibility is reasserted every poll. Active atlas, timer and tooltip
visibility is reapplied before unchanged-value shortcuts. Native text writes retain
their existing qualification checks; reconciliation never restores stale storage.
Tooltip qualification survives only when the current widget and backing pointer
still match. Session invalidation clears the tracked native identities.

The existing build-93847 contract is unchanged: top-level panel lookup RVA
`0x846170`, child lookup RVA `0x856220`, countdown backing pointer offset `0x88`.
Paths: `data/global/ui/layouts/buff-panel/BuffHudhd.json` and
`buff-panel/BuffHud / BuffGrid / BuffSlot00..20 / Countdown`.
The existing byte guards in `src/native/native_contract.hpp` remain authoritative.
No new RVA, offset, hook, signature or compatibility admission is introduced.
Tracking, whitelist mappings, settings, native frame backend and layout coordinates
are unchanged relative to upstream 1.1.0.

## Validation - 2026-10-17

### Automated, upstream 1.1.0 port

Based on upstream `af8d258d25826af7fe178ef55aa4762904114e61` (main after v1.1.0).
Release x64 MSVC build and companion packaging passed; all eight CTest tests passed.
The extended production binding test exercises repeated backing-buffer replacements,
same-allocation reserve restoration, stable-state retention, missing-tree cache
invalidation and repeated visibility resets for all 21 empty slots. These simulate
UI changes; they do not generate actual controller input.
Separate artifact checks passed for the ABI-4 manifest (one DWORD), normal DLL load,
three lifecycle exports, PluginInfo size/identity/version and valid plugin role.
The build retains upstream's existing unused `iconResolved` variable warning.

### Live, equivalent fix on the 1.0.10 review base

The reporter tested review build `1.0.10-controller.1` in D2R Reimagined and reported
consistent visuals when switching input layouts. Screenshots show one active buff
icon and a countdown in both controller (3:09) and keyboard/mouse (2:50) layouts,
without the previous placeholder grid/text overlap.

Read-only process inspection confirmed the mod-scoped review DLL loaded in the game.
The loader identified `1.0.10-controller.1`; its SHA-256 matched the tested artifact:
`d5233410360b7208117257fb3fa928643db32d528b9f053437a5a5f793c44892`.
The buff log confirmed the skill/name resolver and whitelist ready, with no Buff HUD
warnings/errors. Pre-existing level-table startup assertions were also present
before installation and are not attributed to this change.

### Live, upstream 1.1.0 port

The reporter subsequently tested the 1.1.0 build from PR commit
`b84aaac38c538fc16785d4fd1cd71deb136e9ae3` and confirmed switching both ways
between controller and keyboard/mouse looks correct. Read-only process inspection
confirmed the mod-scoped DLL loaded in the running game. Its SHA-256 matched the
built PR candidate:
`1936dde0a4c28677d4c3229066890dd7815910160b79267523670097179b9e17`.
The current loader log identifies Buff HUD 1.1.0; the buff log records successful
initialization, skill/name resolution and 53 enabled whitelist definitions, with
no Buff HUD warnings or errors. Existing unrelated startup diagnostics remain.

This establishes the reported visual switching case on both the review base and
upstream 1.1.0 port. Logs do not emit per-switch events, so they confirm build
identity and initialization rather than an internal branch. Exhaustive
active/resource buffs, expiry/recast, repeated switches, leave/rejoin, performance
and cached-tree reuse without reserve reset remain unverified live. Unknown
native text storage continues to be refused by the existing guards.
