#!/usr/bin/env python3
"""Repository hygiene gate. Never print candidate private values."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

PATHS = re.compile(
    rb"(?i)(?:[a-z]:[\\/](?:users|documents and settings)[\\/]|"
    rb"/(?:users|home)/[^\s/]{2,}/|"
    rb"\\\\(?:users|home)\\\\[^\s]+|"
    rb"\blenovo\b)"
)
SECRET_PATTERNS = (
    ("private_key", re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
    ("provider_key", re.compile(rb"\b(?:gh[pousr]_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{30,}|"
                                 rb"AKIA[A-Z0-9]{16}|AIza[A-Za-z0-9_-]{30,}|sk-[A-Za-z0-9_-]{28,})\b")),
    ("embedded_password_url", re.compile(rb"(?i)(?:postgres(?:ql)?|mysql|mongodb(?:\+srv)?):"
                                         rb"//[^\s:/@]+:[^\s/@]{6,}@")),
    ("auth_header", re.compile(rb"(?i)Authorization\s*:\s*(?:Bearer|Basic)\s+[A-Za-z0-9+/_=-]{24,}")),
)
SENSITIVE_NAMES = re.compile(
    r"(?i)(?:^|/)(?:\.env(?:\.[^/]+)?|\.netrc|id_rsa|id_ed25519|secrets|credentials)"
    r"(?:$|/)|\.(?:p12|pfx|key|pem|elf)$"
)
# A known test vector is public by design. Its bytes are not a production key.
TEST_VECTOR_PATHS = {"docs/lora-v1-vector.json", "tests/native.cpp", "tools/check_lora.py"}
MAX_DISPLAY = 40


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL)


def scan(path: str, data: bytes) -> list[str]:
    found = []
    if path != "tools/security_audit.py" and PATHS.search(data):
        found.append("personal_machine_path")
    if path not in TEST_VECTOR_PATHS:
        for name, pattern in SECRET_PATTERNS:
            if pattern.search(data):
                found.append(name)
    if SENSITIVE_NAMES.search(path):
        found.append("sensitive_filename")
    return found


def main() -> int:
    paths = [p.decode() for p in git("ls-files", "-z").split(b"\0") if p]
    hits = []
    for path in paths:
        for name in scan(path, Path(path).read_bytes()):
            hits.append((path, name))
    for path, label in hits[:MAX_DISPLAY]:
        print(f"FAIL {path}: {label}", file=sys.stderr)
    if len(hits) > MAX_DISPLAY:
        print(f"... {len(hits) - MAX_DISPLAY} further hits (redacted)", file=sys.stderr)
    print(f"Current tracked files scanned: {len(paths)}. Findings: {len(hits)}.")
    if "--history" in sys.argv:
        historic = 0
        flagged = 0
        for row in git("rev-list", "--objects", "--all").splitlines():
            sha, _, hint = row.partition(b" ")
            if not hint or git("cat-file", "-t", sha.decode()).strip() != b"blob":
                continue
            size = int(git("cat-file", "-s", sha.decode()))
            if size > 20_000_000:
                continue
            historic += 1
            if PATHS.search(git("cat-file", "blob", sha.decode())):
                flagged += 1
        # Existing historical leaks cannot be removed by an ordinary commit.
        # Force-pushing rewritten history has irreversible effects on clones.
        print(f"Reachable history: {historic} named blobs reviewed; "
              f"{flagged} blobs contain private-path indicators (report only).")
    return 1 if hits else 0


if __name__ == "__main__":
    raise SystemExit(main())
