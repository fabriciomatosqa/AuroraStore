#!/usr/bin/env python3
"""Decode the licensed TrueType font stored as text for GitHub API uploads."""
from __future__ import annotations

import base64
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENCODED = ROOT / "assets" / "fonts" / "Gontserrat-Regular.ttf.b64"
TARGET = ROOT / "assets" / "fonts" / "Gontserrat-Regular.ttf"

data = base64.b64decode("".join(ENCODED.read_text(encoding="ascii").split()), validate=True)
if not data.startswith(b"\x00\x01\x00\x00"):
    raise SystemExit("The font file is not a valid TrueType file")
TARGET.write_bytes(data)
print(f"Prepared {TARGET.relative_to(ROOT)} ({len(data)} bytes)")
