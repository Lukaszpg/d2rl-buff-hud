"""The legacy layout must expose exactly the widget tree of the HD layout.

BuffHud resolves widgets by name and qualifies the icon frames, countdown and
tooltip buffers per slot, so both graphics modes need the same names, the same
per-slot declared frames and the same reserve strings. Legacy sprites are the
classic DC6 files under data/global/ui, addressed with backslash paths.
"""
import json
import sys
from pathlib import Path


def tree(node, path=""):
    here = f"{path}/{node['name']}"
    yield here, node
    for child in node.get("children", []):
        yield from tree(child, here)


hd = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
legacy = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
hd_nodes = dict(tree(hd))
legacy_nodes = dict(tree(legacy))
assert list(hd_nodes) == list(legacy_nodes), "legacy layout widget tree differs from HD"

for path, node in hd_nodes.items():
    other = legacy_nodes[path]
    assert node["type"] == other["type"], path
    fields, legacy_fields = node.get("fields", {}), other.get("fields", {})
    for key in ("normalFrame", "pressedFrame", "frame", "text", "tooltipString", "visible", "focusOnMouseOver"):
        assert fields.get(key) == legacy_fields.get(key), f"{path}: {key} differs"
    if "filename" in legacy_fields:
        name = legacy_fields["filename"]
        assert "/" not in name and "\\" in name, f"{path}: not a legacy DC6 path: {name}"

# Item_<code> pictures are hidden ImageWidgets that every slot declares alike.
slots = [node for node in hd_nodes.values() if node["name"].startswith("BuffSlot")]
item_sets = {tuple(c["name"] for c in slot["children"] if c["name"].startswith("Item_")) for slot in slots}
assert len(slots) == 21 and len(item_sets) == 1, "every slot must declare the same Item_ widgets"
for path, node in hd_nodes.items():
    if node["name"].startswith("Item_"):
        code = node["name"][len("Item_"):]
        assert 1 <= len(code) <= 4 and code.isalnum(), f"{path}: item code must be 1-4 letters or digits"
        assert node["type"] == "ImageWidget" and node["fields"].get("visible") is False, path
print("PASS: legacy layout matches the HD widget contract")
