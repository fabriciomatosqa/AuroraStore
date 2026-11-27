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
    icon_rgb = icon.copy()
Path("sce_sys").mkdir(parents=True, exist_ok=True)
Path("sce_sys/icon0.png").write_bytes(icon_bytes)

# The PS4 renderer consumes compact RGBA data, so it needs no PNG decoder.
resampling = getattr(Image, "Resampling", Image).LANCZOS
small = icon_rgb.resize((256, 256), resampling)
rgba = []
for red, green, blue in small.getdata():
    alpha = max(red, green, blue)
    if alpha == 0:
        rgba.append((0, 0, 0, 0))
    else:
        rgba.append((
            min(255, round(red * 255 / alpha)),
            min(255, round(green * 255 / alpha)),
            min(255, round(blue * 255 / alpha)),
            alpha,
        ))
logo = Image.new("RGBA", (256, 256))
logo.putdata(rgba)
Path("assets/aurora-logo.rgba").write_bytes(logo.tobytes())
print("Restored package icon and generated transparent 256x256 Aurora logo")
