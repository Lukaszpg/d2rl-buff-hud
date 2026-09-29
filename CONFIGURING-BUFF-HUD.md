# Buff Panel 1.0.11 — buff-hud.txt

The runtime override is optional. Without it, Buff Panel uses the catalog embedded in `src/systems/buff_tracker/buff_tracker.cpp`.

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
