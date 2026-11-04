#!/usr/bin/env python3
"""Compile gateway + exact pinned node header together, then exercise wire interop."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parent.parent
peer = Path(sys.argv[1]) if len(sys.argv) > 1 else root / ".peer"
node_header = peer / "firmware/include/lora_protocol.h"
assert node_header.is_file(), f"Missing read-only node checkout: {node_header}"
build = root / ".build"
build.mkdir(exist_ok=True)
target = build / "gateway-node-wire"
cmd = [
    "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
    "-Wno-misleading-indentation", "-Ifirmware/include",
    "-Ifirmware/include/core", f"-I{node_header.parent}",
    "tests/ptmp_interop.cpp", "firmware/src/core/crypto.cpp",
    "firmware/src/core/protocol.cpp", "-o", str(target)
]
subprocess.run(cmd, check=True, cwd=root)
subprocess.run([str(target)], check=True, cwd=root)
print(f"PASS: direct gateway production decoder and read-only peer header {node_header}")
