#!/usr/bin/env python3
"""Cross-check the C3 board adapter, wiring CSV and repository SVG diagrams."""
import csv
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "firmware/include/board/reference.h").read_text(encoding="utf-8")
nets = {
    "SCK": ("sck", "4", "12"),
    "MOSI": ("mosi", "6", "14"),
    "MISO": ("miso", "5", "13"),
    "NSS": ("cs", "7", "15"),
    "RESET": ("reset", "10", "4"),
    "BUSY": ("busy", "3", "10"),
    "DIO1": ("dio1", "1", "6"),
}
with (ROOT / "docs/wiring-c3.csv").open(newline="", encoding="utf-8") as f:
    rows = list(csv.DictReader(f))
index = {x["net"]: x for x in rows}
for signal, (symbol, gpio, pad) in nets.items():
    assert re.search(r"\b" + symbol + r"\s*=\s*" + gpio + r"\b", source), signal
    assert signal in index and index[signal]["esp32_c3_gpio_or_supply"] == "GPIO" + gpio
    assert index[signal]["ra_01sh_pad"] == pad
assert "tcxoVoltage = 0.0f" in source
assert "dio2RfSwitch = true" in source
for filename in ["wiring-c3.svg", "ptmp-topology.svg", "poll-sequence.svg",
                 "platformio-layout.svg"]:
    file = ROOT / "docs/assets" / filename
    root = ET.parse(file).getroot()
    assert root.tag.endswith("svg") and root.attrib.get("viewBox"), filename
for doc in ["README.md", "README.id.md", "docs/PLATFORMIO_VSCODE.md"]:
    assert (ROOT / doc).is_file()
print("PASS: gateway board GPIO, module pads, SVG diagrams and document links verified")
