# Buff HUD 1.0.11

Requires D2RLoader with PluginSDK 0.3.0 services / plugin ABI 4. Native hooks remain qualified only for D2R build 93847.

With the game closed, copy **both** files from `d2rloader/plugins/` into your selected scope:

- Ladder: `<game>/mods/ReimaginedLadder/d2rloader/plugins/`
- Another mod: `<game>/mods/<mod-name>/d2rloader/plugins/`
- Global: `<game>/d2rloader/plugins/`

Keep `d2rl-buff-panel.dll` and `d2rl-buff-panel.mpq` together. Install in one scope only. Include this pair in the ladder bundle; no files need to be added to the mod's MPQ. Keep the included license notices with redistributions.

The companion contains the panel layout and default buff whitelist. D2RLoader compiles the custom TXT table at load time; no linked compiler/game installation or `-txt` launch flag is needed by the recipient. Existing active-mod overrides at the same virtual paths take priority.

Start the game and use `buff-panel status` for the compact production status line. Verify real Fade, Shout/Battle Orders/Battle Command, and resource buffs such as Bone Armor in game.

This fork preserves the upstream author's attribution. Packaging/build verification is not live-game qualification.
