# Buff Panel 1.0.11 validation

Production-cleanup validation performed on the packaged source tree:

- All production `.cpp` files pass C++20 syntax checking against the captured ABI-4 PluginSDK with Windows calling conventions neutralized for the non-Windows validation host.
- Timer metadata regression test passes, including Shout/BO/BC configured-skill fallback and finite native expiry semantics.
- Skills→SkillDesc resolver regression test passes, including ID/index collision, reordered descriptors, ambiguity rejection and BO/BC icon/name witnesses.
- Loose `buff-hud.txt` override test passes.
- Loose layout override test passes against the 1.0.10/1.0.11 default layout bytes.
- Portable tooltip storage check confirms 126-byte payload + NUL remains within the 127-byte qualified reserve.
- Source scan confirms the production runtime no longer contains fake-buff/debug-clock code, mouse A/B modes, `buff-panel-tracker`, `buff-panel-status`, manual icon rebuild, display clear, hot-path telemetry counters, unused stat-read interceptors, or unused PluginSDK service queries.

A real Windows/MSVC build and in-game D2R build-93847 qualification are still required before promotion.
