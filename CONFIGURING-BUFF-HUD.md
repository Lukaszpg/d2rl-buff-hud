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

## buff-hud-display.txt — icon and name overrides

Buff HUD normally takes a buff's icon and tooltip name from its source skill. When a state has no
usable skill (a custom state, or a skill whose description has no icon), add a row to the optional
presentation table:

```text
<game>/mods/<mod>/<mod>.mpq/data/global/excel/d2rloader/buff-panel/buff-hud-display.txt
```

The exact tab-separated columns are:

```text
state_id	icon_atlas	icon_frame	name_key	name	item_code
```

- `state_id` is a state already tracked through `buff-hud.txt` (1-4095). Each state appears at most once.
- `icon_atlas` is one of `amazon`, `sorceress`, `necromancer`, `paladin`, `barbarian`, `druid`,
  `assassin`, `warlock` or `global` (any case). Empty keeps the automatic icon.
- `icon_frame` is the frame inside that atlas. It needs `icon_atlas`; `65535` is reserved.
- `name_key` is an optional localization string key. When it resolves, it is the tooltip name.
- `name` is the literal tooltip name used when `name_key` is empty or does not resolve (up to 63 ASCII characters).
- `item_code` is an optional item code (1-4 letters or digits, e.g. `hp5`). When the layout has an
  `Item_<code>` widget in the slot, that item's inventory picture replaces the icon; otherwise the
  atlas icon is used.

Rows that leave a field empty keep the automatic value for that field, so a state without a row
behaves exactly as before. The table is presentation only: it never adds or removes tracked buffs,
and an invalid table is logged and ignored instead of disabling the HUD.

Example (stock states; every row has all six columns, empty fields included):

```text
state_id	icon_atlas	icon_frame	name_key	name	item_code
32	barbarian	26			
51			MyBattleCommandName	Battle Command	
```

The companion ships a header-only default, which means no overrides.

### Item pictures

Both layouts declare one hidden `Widget` per item code in every `BuffSlot`, named `Item_<code>`,
holding one `ImageWidget` with the picture (an `ImageWidget` ignores `"visible": false`, so the
container is what BuffHud shows and hides).
The shipped layouts cover the stock potions `hp1`-`hp5`, `mp1`-`mp5`, `rvs`, `rvl`, `vps`, `yps` and
`wms`. The HD widget points at the item's HD inventory sprite (for example
`Items/misc/potion/greater_healing_potion`), the legacy widget at its inventory DC6 (for example
`..\\items\\invhp5`). To use another item, add an `Item_<code>` container with the same name to all 21
slots of both `BuffHudhd.json` and `BuffHud.json`; `layout_parity` checks that both layouts agree.
