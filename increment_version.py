import re
from pathlib import Path

VERSION_CPP = Path("version.cpp")

text = VERSION_CPP.read_text()

match = re.search(
    r'(const\s+uint32_t\s+FW_BUILD_NUMBER\s*=\s*)(\d+)',
    text
)

if not match:
    raise RuntimeError("FW_BUILD_NUMBER not found")

current = int(match.group(2))
new = current + 1

updated = re.sub(
    r'(const\s+uint32_t\s+FW_BUILD_NUMBER\s*=\s*)\d+',
    rf'\g<1>{new}',
    text,
    count=1
)

VERSION_CPP.write_text(updated)

print(f"FW_BUILD_NUMBER: {current} -> {new}")
