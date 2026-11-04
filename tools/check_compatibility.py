#!/usr/bin/env python3
"""PlatformIO / radio contract gate for the read-only upstream node.

The lock documents the independently inspected node commit. An optional
node.ini path checks a local clone without modifying it.
"""
from __future__ import annotations
import argparse
import configparser
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

def read_ini(path: Path) -> configparser.ConfigParser:
    ini = configparser.ConfigParser(interpolation=None)
    with path.open(encoding="utf-8") as stream:
        ini.read_file(stream)
    return ini

def dependencies(ini: configparser.ConfigParser, section: str) -> dict[str, str]:
    lines = ini.get(section, "lib_deps").strip().splitlines()
    found: dict[str, str] = {}
    for line in lines:
        entry = line.strip()
        if not entry:
            continue
        if "@" not in entry:
            raise AssertionError(f"Unpinned dependency: {entry}")
        name, version = entry.rsplit("@", 1)
        found[name] = version
    return found

def validate(node_platformio: Path | None = None) -> None:
    lock = json.loads((ROOT / "docs/upstream-package-lock.json").read_text(encoding="utf-8"))
    gateway = read_ini(ROOT / "platformio.ini")
    env = "env:esp32c3"
    assert gateway.get("platformio", "default_envs") == "esp32c3"
    assert gateway.get("platformio", "src_dir") == "firmware"
    assert gateway.get("platformio", "include_dir") == "firmware"
    assert gateway.get(env, "board") == lock["gateway_board"]
    assert gateway.get(env, "framework") == lock["framework"]
    assert gateway.get(env, "platform") == lock["platform"]
    assert gateway.get(env, "board_build.partitions") == "partitions.csv"
    assert gateway.get(env, "board_build.filesystem") == "littlefs"
    assert gateway.get(env, "board_upload.maximum_size") == "2097152"
    got = dependencies(gateway, env)
    for name, expected in lock["shared_libraries"].items():
        assert got.get(name) == expected, f"Shared library mismatch: {name}"
    assert "GATEWAY_BOARD_C3_DEVKITM" in gateway.get(env, "build_flags")
    if node_platformio:
        node = read_ini(node_platformio)
        upstream = "env:esp32s3"
        assert node.get(upstream, "board") == lock["upstream_board"]
        assert node.get(upstream, "framework") == lock["framework"]
        assert node.get(upstream, "platform") == lock["platform"]
        for name, expected in lock["shared_libraries"].items():
            assert dependencies(node, upstream).get(name) == expected, f"Node mismatch: {name}"
    codec = (ROOT / "firmware/core/protocol.h").read_text(encoding="utf-8")
    for signature in ["HeaderSize = 24", "TagSize = 16", "MaxPayload = 112",
                      "Hello = 1", "Ack = 2", "Telemetry = 3", "Poll = 4",
                      "Checkpoint = 5"]:
        assert signature in codec, f"Protocol contract mismatch: {signature}"
    vector = json.loads((ROOT / lock["reference_fixture"]).read_text(encoding="utf-8"))
    assert len(bytes.fromhex(vector["key_hex"])) == 32
    assert bytes.fromhex(vector["packet_hex"]) == (
        bytes.fromhex(vector["body_hex"]) + bytes.fromhex(vector["tag_hex"]))
    assert len(bytes.fromhex(vector["tag_hex"])) == 16
    assert (ROOT / "firmware/board/reference.h").exists()
    print("NOTICE: shared factory carrier 915 MHz is for software compatibility only; "
          "do not enable it for Indonesia LPWAN. Configure both endpoints to a "
          "permitted 920-923 MHz carrier, e.g. 921 MHz, before TX.")
    print("PASS: gateway ESP32-C3 PlatformIO packages, wire contract and vector agree "
          "with pinned read-only S3 node reference")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--node-platformio", type=Path, default=None,
                        help="Optional path to local node's firmware/platformio.ini")
    args = parser.parse_args()
    validate(args.node_platformio)
