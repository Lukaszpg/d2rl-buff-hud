"""Every Item_<code> picture of a layout must be one BuffHud hides by name.

The game does not apply a layout's "visible": false and widgets cannot be
enumerated, so BuffHud hides the codes of LayoutItemCodes in buff_hud.cpp and
every item_code of buff-hud-display.txt. A picture for any other code would be
drawn in every slot.

usage: item_pictures_test.py <buff_hud.cpp> <buff-hud-display.txt> <layout>...
"""
import json
import re
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text(encoding="utf-8")
block = re.search(r"LayoutItemCodes\{(.*?)\};", source, re.S)
assert block, "LayoutItemCodes not found"
known = set(re.findall(r'"([A-Za-z0-9]{1,4})"', block.group(1)))

rows = Path(sys.argv[2]).read_text(encoding="ascii").splitlines()
header = rows[0].split("\t")
column = header.index("item_code")
known |= {row.split("\t")[column] for row in rows[1:] if row.strip() and row.split("\t")[column]}


def names(node):
    yield node["name"]
    for child in node.get("children", []):
        yield from names(child)


for layout in sys.argv[3:]:
    tree = json.loads(Path(layout).read_text(encoding="utf-8"))
    codes = {name[len("Item_"):] for name in names(tree) if name.startswith("Item_")}
    unknown = sorted(codes - known)
    assert not unknown, f"{layout}: Item_ pictures BuffHud does not hide: {unknown}"
print(f"PASS: {len(known)} item codes cover every Item_ picture")
