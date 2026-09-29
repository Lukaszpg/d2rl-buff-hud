"""Optional archive round-trip check: python -m pip install mpyq==0.2.5."""

import argparse
import hashlib
from pathlib import Path

import mpyq


def verify(source: Path, archive: Path) -> None:
    expected = {
        "data/global/ui/layouts/buff-panel/BuffHudhd.json",
        "data/global/excel/d2rloader/buff-panel/buff-hud.txt",
    }
    # D2RLCompiler encrypts its listfile. mpyq can read the payloads and tables
    # but not encrypted files, so verify all live blocks by their known paths.
    packed = mpyq.MPQArchive(str(archive), listfile=False)
    expected_blocks = set()
    for name in expected | {"(listfile)"}:
        entry = packed.get_hash_table_entry(name)
        if entry is None:
            raise ValueError(f"Missing archive entry: {name}")
        expected_blocks.add(entry.block_table_index)
    live_blocks = {i for i, entry in enumerate(packed.block_table) if entry.flags & mpyq.MPQ_FILE_EXISTS}
    if live_blocks != expected_blocks or len(expected_blocks) != len(expected) + 1:
        raise ValueError("Archive contains extra or aliased files")
    for name in sorted(expected):
        payload = packed.read_file(name)
        original = (source / name).read_bytes()
        if payload != original:
            raise ValueError(f"Archive bytes differ from source: {name}")
        print(f"PASS {name}: {len(payload)} bytes, SHA256 {hashlib.sha256(payload).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1] / "companion")
    args = parser.parse_args()
    verify(args.source, args.archive)
