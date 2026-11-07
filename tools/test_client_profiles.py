#!/usr/bin/env python3
"""Compile actual read-only current client profile source against the gateway's ID contract."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
peer = Path(sys.argv[1]) if len(sys.argv) > 1 else root / ".peer"
header = peer / "firmware/lib/FortMeter/src/MeterProfiles.h"
assert header.is_file(), f"Missing client MeterProfiles.h: {header}"
build = root / ".build"
build.mkdir(exist_ok=True)
target = build / "gateway-client-profiles"
cmd = [
    "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
    "-Wno-misleading-indentation", "-Ifirmware/include", "-Ifirmware/include/core",
    f"-I{header.parent}", "tests/current_client_profiles.cpp", "-o", str(target),
]
subprocess.run(cmd, check=True, cwd=root)
subprocess.run([str(target)], check=True, cwd=root)
