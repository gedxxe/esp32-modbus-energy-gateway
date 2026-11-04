#!/usr/bin/env python3
"""Enforce conventional firmware/ PlatformIO workspace and include ownership."""
from pathlib import Path
import re
import configparser

ROOT = Path(__file__).resolve().parent.parent
FW = ROOT / "firmware"
assert not (ROOT / "platformio.ini").exists(), "Root PlatformIO config must not exist"
assert (FW / "platformio.ini").is_file()
assert (FW / "partitions.csv").is_file()
assert (FW / ".vscode/extensions.json").is_file()
assert (FW / ".vscode/settings.json").is_file()
assert (FW / "include/web/assets.h").is_file()
cfg = configparser.ConfigParser(interpolation=None)
cfg.read(FW / "platformio.ini", encoding="utf-8")
assert cfg.get("platformio", "src_dir", fallback="src") == "src"
assert cfg.get("platformio", "include_dir", fallback="include") == "include"
assert cfg.get("env:esp32c3", "board") == "esp32-c3-devkitm-1"
assert cfg.get("env:esp32c3", "board_build.partitions") == "partitions.csv"
assert "post:../tools/pio_flash_guard.py" in cfg.get("env:esp32c3", "extra_scripts")
assert (FW / "src/main.cpp").is_file()
sources = sorted((FW / "src").rglob("*.cpp"))
headers = sorted((FW / "include").rglob("*.h"))
assert len(sources) >= 12 and len(headers) >= 15
# Legacy directories may not be reintroduced as parallel C++ compilation units.
for folder in ("app", "core", "storage", "board", "radio", "web"):
    assert not list((FW / folder).glob("*.cpp"))
    assert not list((FW / folder).glob("*.h"))
check = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
includes = [FW / "include"] + [FW / "include" / x for x in
           ("app", "core", "storage", "web", "board")]
for source in sources + headers:
    content = source.read_text(encoding="utf-8")
    for name in check.findall(content):
        if not any((parent / name).is_file() for parent in [source.parent, *includes]):
            raise AssertionError(f"Unresolved project include in {source.relative_to(ROOT)}: {name}")
print(f"PASS: firmware/ PlatformIO workspace, {len(sources)} recursively compiled sources, "
      f"{len(headers)} headers and all local includes resolved")
