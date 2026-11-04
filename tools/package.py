"""Build a release from the exact tracked source and compiled ESP32-C3 images.

ELF/debug symbols are deliberately excluded: they may contain absolute paths
from the machine that built them. The packaged LittleFS image is for INITIAL
provisioning ONLY; flashing it on an active gateway replaces stored archives.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / ".pio/build/esp32c3"
RELEASE = ROOT / "release"
VERSION = "0.2.0-rc1"
ARCHIVE = RELEASE / f"midea-engineering-gateway-c3-{VERSION}.zip"
MANIFEST = RELEASE / "manifest.json"
BINARIES = (
    ("bootloader.bin", "0x0"),
    ("partitions.bin", "0x8000"),
    ("firmware.bin", "0x20000"),
    ("littlefs.bin", "0x220000"),
)

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def path_string(p: Path) -> str:
    return str(p.relative_to(ROOT)).replace("\\", "/")

def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT).decode().strip()

def tracked_sources() -> list[Path]:
    # Version-controlled sources, docs, diagrams and .vscode settings only.
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT)
    return sorted(ROOT / p.decode("utf-8") for p in tracked.split(b"\0")
                  if p and not p.decode("utf-8").startswith("release/"))

def metadata(source: list[Path]) -> dict:
    lock = json.loads((ROOT / "docs/upstream-package-lock.json").read_text())
    return {
        "release": VERSION,
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "source_commit": git("rev-parse", "HEAD"),
        "node_reference_commit": lock["upstream_commit"],
        "node_modified": False,
        "target": lock["gateway_board"],
        "framework": lock["framework"],
        "platform": lock["platform"],
        "libraries": lock["shared_libraries"],
        "protocol": lock["contract"],
        "flash_bytes": 4194304,
        "hardware_tested": False,
        "deployment": "UNTESTED: bench and four-node PTMP validation required",
        "safety": "Do not upload littlefs.bin on an installed gateway; archives are overwritten. Preserve gw_nvs at 0xE000.",
        "artifacts": [],
        "source": [{"path": path_string(p), "sha256": digest(p.read_bytes())} for p in source],
    }

def verify() -> None:
    obj = json.loads(MANIFEST.read_text())
    with zipfile.ZipFile(ARCHIVE) as z:
        assert z.testzip() is None
        for row in obj["source"] + obj["artifacts"]:
            path = ROOT / row["path"]
            data = path.read_bytes()
            assert digest(data) == row["sha256"], path
            if "bytes" in row:
                assert len(data) == row["bytes"], path
            assert digest(z.read(row["path"])) == row["sha256"], row["path"]
        assert z.read("release/manifest.json") == MANIFEST.read_bytes()
        assert not any(name.endswith((".elf", ".map", ".pem", ".key")) for name in z.namelist())
    print(f"PASS: {len(obj['source'])} tracked sources and {len(obj['artifacts'])} C3 flash-image hashes")

def create() -> None:
    RELEASE.mkdir(exist_ok=True)
    src = tracked_sources()
    obj = metadata(src)
    for name, offset in BINARIES:
        p = BUILD / name
        if not p.is_file():
            raise RuntimeError(f"Missing PlatformIO build artifact {name}; build firmware and buildfs first")
        data = p.read_bytes()
        if name in ("bootloader.bin", "firmware.bin"):
            assert data and data[0] == 0xE9, f"{name} is not an ESP32 image"
            assert struct.unpack_from("<H", data, 12)[0] == 5, "Expected ESP32-C3 chip ID"
        if name == "firmware.bin":
            assert len(data) <= 0x200000, "Application exceeds factory partition"
        if name == "littlefs.bin":
            assert len(data) == 0x1E0000, "Incorrect archive filesystem partition size"
        target = RELEASE / name
        target.write_bytes(data)
        obj["artifacts"].append({"path": path_string(target), "bytes": len(data),
                                 "sha256": digest(data), "flash_offset": offset})
    MANIFEST.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    to_pack = src + [RELEASE / n for n, _ in BINARIES] + [MANIFEST]
    with zipfile.ZipFile(ARCHIVE, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for p in to_pack:
            info = zipfile.ZipInfo(path_string(p), (2026, 11, 5, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            zipped.writestr(info, p.read_bytes())
    (RELEASE / "SHA256SUMS.txt").write_text(
        "".join(f"{digest(p.read_bytes())}  {p.name}\n"
                for p in (ARCHIVE, MANIFEST, *[RELEASE / n for n, _ in BINARIES])),
        encoding="utf-8")
    verify()
    print(f"Release generated locally: {ARCHIVE.name}; no ELF or workstation paths in debug symbols")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    opts = parser.parse_args()
    if opts.verify:
        verify()
    else:
        create()
