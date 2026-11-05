# Buff HUD 1.0.11 — buff-hud.txt

The runtime override is optional. Without it, Buff HUD uses the catalog embedded in `src/systems/buff_tracker/buff_tracker.cpp`.

Place an override at:

```text
<game>/mods/<mod>/<mod>.mpq/data/global/excel/d2rloader/buff-panel/buff-hud.txt
```

The exact tab-separated columns are:

```text
name	state_id	display_type	value_stat	max_stat	skill_id	value_shift	enabled
```

There is intentionally **no skill-level field**.

For `display_type=timer`:
- `value_stat`, `max_stat`, and `value_shift` must be `0`.
- `skill_id=0` means skill identity must come from the native StatList.
- `skill_id!=0` is a fallback only; a nonzero native StatList skill ID always wins.
- A valid finite future native expiry is required. There is no state-presence-only timer path.

For `display_type=resource`, `value_stat`, `max_stat`, and `skill_id` are required as before.

The embedded defaults include:

```text
shout	26	timer	0	0	138	0	1
battle_orders	32	timer	0	0	149	0	1
battle_command	51	timer	0	0	155	0	1
```

## buff-panel.toml — hiding the icons behind game panels

D2RLoader writes the plugin's default settings to `d2rloader\config\buff-panel.toml` (next to the plugin's own
`d2rloader` folder: the game folder, or the mod folder for a mod-scoped install) the first time it loads:

```toml
[buff_panel]
hide_with_panels = true
```

`hide_with_panels = true` (the default; a file without the key keeps it) hides the icons while the stash or the
Horadric Cube is open, and shows them again when it closes. A side panel (character, quests, waypoints, a vendor, trade,
the mercenary, the party; inventory, skill tree) hides them only while it overlaps an icon on screen; only the top row of
the grid reaches a right panel, so a player can fight with the inventory open. `false` keeps them shown always.

The plugin reads the game's panel visibility through a bit it learns once by toggling a slot's `Countdown`
through the WidgetService; the log line `native visibility bit qualified|not-qualified` reports the result (when it is
not qualified, nothing hides). `buff-panel status` reports `hideWithPanels=on|off|not-qualified` and `hiddenBy=<panel>`.
