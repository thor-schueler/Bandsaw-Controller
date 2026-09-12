#!/usr/bin/env python3

import re
from pathlib import Path

VERSION_H = Path("version.h")

text = VERSION_H.read_text()

match = re.search(
    r'(#define\s+FW_BUILD_NUMBER\s+)(\d+)',
    text
)

if not match:
    raise RuntimeError("FW_BUILD_NUMBER not found")

current = int(match.group(2))
new = current + 1

updated = re.sub(
    r'(#define\s+FW_BUILD_NUMBER\s+)\d+',
    rf'\g<1>{new}',
    text,
    count=1
)

VERSION_H.write_text(updated)

print(f"FW_BUILD_NUMBER: {current} -> {new}")