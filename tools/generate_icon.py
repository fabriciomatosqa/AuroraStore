#!/usr/bin/env python3
"""Restore the supplied Aurora icon as the PS4 package icon."""
import base64
from pathlib import Path
from io import BytesIO
from PIL import Image

source = Path("assets/aurora-icon.png.b64")
icon_bytes = base64.b64decode(source.read_text(encoding="ascii").strip(), validate=True)
with Image.open(BytesIO(icon_bytes)) as icon:
    assert icon.format == "PNG", icon.format
    assert icon.size == (512, 512), icon.size
    assert icon.mode == "RGB", icon.mode
Path("sce_sys").mkdir(parents=True, exist_ok=True)
Path("sce_sys/icon0.png").write_bytes(icon_bytes)
print("Restored supplied icon: exact PNG bytes, 512x512 RGB")
