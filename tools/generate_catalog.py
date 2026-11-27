#!/usr/bin/env python3
"""Generate the C++ catalog table used by the PS4 storefront."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "catalogs" / "homebrew.json"
TARGET = ROOT / "src" / "catalog_data.h"


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


catalog = json.loads(SOURCE.read_text(encoding="utf-8"))
items = catalog.get("items", [])
if not items:
    raise SystemExit("The homebrew catalog is empty")

required = ("id", "name", "version", "developer", "category", "description",
            "license", "repository", "releases", "is_tool")
lines = [
    "// Generated from catalogs/homebrew.json. Do not edit manually.",
    "#pragma once",
    "struct CatalogEntry {",
    "    const char* id;",
    "    const char* name;",
    "    const char* version;",
    "    const char* developer;",
    "    const char* category;",
    "    const char* description;",
    "    const char* license;",
    "    const char* repository;",
    "    const char* releases;",
    "    bool isTool;",
    "};",
    "static const CatalogEntry kCatalogItems[] = {",
]
for item in items:
    missing = [key for key in required if key not in item]
    if missing:
        raise SystemExit(f"Catalog item {item.get('id', '<unknown>')} misses {missing}")
    fields = [cpp_string(str(item[key])) for key in required[:-1]]
    fields.append("true" if item["is_tool"] else "false")
    lines.append("    {" + ", ".join(fields) + "},")
lines.extend([
    "};",
    "static constexpr int kCatalogCount = sizeof(kCatalogItems) / sizeof(kCatalogItems[0]);",
    "",
])
TARGET.write_text("\n".join(lines), encoding="utf-8")
print(f"Generated {TARGET.relative_to(ROOT)} with {len(items)} entries")
