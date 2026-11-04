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

# Item_<code> pictures are hidden Widgets around one ImageWidget (an ImageWidget
# ignores "visible": false), declared alike by every slot.
slots = [node for node in hd_nodes.values() if node["name"].startswith("BuffSlot")]
item_sets = {tuple(c["name"] for c in slot["children"] if c["name"].startswith("Item_")) for slot in slots}
assert len(slots) == 21 and len(item_sets) == 1, "every slot must declare the same Item_ widgets"
for path, node in hd_nodes.items():
    if node["name"].startswith("Item_"):
        code = node["name"][len("Item_"):]
        assert 1 <= len(code) <= 4 and code.isalnum(), f"{path}: item code must be 1-4 letters or digits"
        assert node["type"] == "Widget" and node["fields"].get("visible") is False, path
        assert [c["type"] for c in node.get("children", [])] == ["ImageWidget"], path

# No slot may draw into another: each slot's box and its Countdown stay apart.
# Legacy DC6 skill icons draw at their own 48 px whatever their "scale", so the
# legacy slot box must hold a full icon.
def footprints(layout):
    grid = next(c for c in layout["children"] if c["name"] == "BuffGrid")
    for slot in grid["children"]:
        r = slot["fields"]["rect"]
        boxes = [(r["x"], r["y"], r["width"], r["height"])]
        for child in slot["children"]:
            c = child["fields"].get("rect", {})
            if "width" in c:
                boxes.append((r["x"] + c["x"], r["y"] + c["y"], c["width"], c["height"]))
        yield slot["name"], boxes


def overlaps(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


for name, layout in (("HD", hd), ("legacy", legacy)):
    slots = list(footprints(layout))
    for i, (first, first_boxes) in enumerate(slots):
        for second, second_boxes in slots[i + 1:]:
            assert not any(overlaps(a, b) for a in first_boxes for b in second_boxes), \
                f"{name}: {first} overlaps {second}"
for path, node in legacy_nodes.items():
    if node["name"].startswith("Icon"):
        slot = legacy_nodes[path.rsplit("/", 1)[0]]["fields"]["rect"]
        assert slot["width"] >= 48 and slot["height"] >= 48, f"{path}: legacy slot smaller than a 48 px icon"
print("PASS: legacy layout matches the HD widget contract")
