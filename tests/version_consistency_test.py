from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
assert re.fullmatch(r"\d+\.\d+\.\d+", version), f"Invalid VERSION: {version!r}"
major, minor, patch = version.split(".")

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
assert f"project(BuffPanel VERSION {version} LANGUAGES CXX RC)" in cmake

plugin = (ROOT / "src/plugin.cpp").read_text(encoding="utf-8")
assert f'.version = "{version}"' in plugin
assert f"Buff Panel {version} requires" in plugin
assert f"Buff Panel {version} loaded" in plugin

rc = (ROOT / "src/plugin.rc").read_text(encoding="utf-8")
numeric = f"{major},{minor},{patch},0"
assert f"FILEVERSION {numeric}" in rc
assert f"PRODUCTVERSION {numeric}" in rc
assert f'VALUE "FileVersion", "{version}"' in rc
assert f'VALUE "ProductVersion", "{version}"' in rc

for relative in (
    "src/systems/buff_hud/buff_hud.cpp",
    "src/systems/buff_tracker/buff_tracker.cpp",
):
    text = (ROOT / relative).read_text(encoding="utf-8")
    versions = set(re.findall(r"Buff Panel (\d+\.\d+\.\d+)", text))
    assert versions == {version}, f"{relative}: runtime versions {sorted(versions)} != {version}"

print(f"PASS: VERSION {version} matches CMake, DLL metadata, resources and runtime logs")
