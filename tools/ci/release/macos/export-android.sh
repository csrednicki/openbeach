#!/bin/bash
# Packs the imported game data (~/Library/Application Support/OpenBeach/userdata/) into a STORE-mode openbeach-data.zip
# for OpenBeach on Android (docs/build/releases.md).
#   /Applications/OpenBeach.app/Contents/Resources/export-android.sh [output.zip]
set -euo pipefail

userdata="$HOME/Library/Application Support/OpenBeach/userdata"
output="${1:-$HOME/Desktop/openbeach-data.zip}"

if [[ ! -f "$userdata/packs/base/pack.json" ]]; then
    echo "error: no imported game data in $userdata/packs/base/pack.json" >&2
    echo "Start OpenBeach and import your own copy of Beach Life first, then run this script again." >&2
    exit 1
fi

python3 - "$userdata" "$output" <<'PY'
import sys, zipfile
from pathlib import Path

userdata = Path(sys.argv[1])
output = Path(sys.argv[2])
temp = output.with_suffix(output.suffix + ".tmp")

files = []
for rel in ("packs/base", "remaster", "SaveGames"):
    root = userdata / rel
    if root.is_dir():
        files.extend(sorted(p for p in root.rglob("*") if p.is_file()))
for name in ("BeachLife.ini", "openbeach.ini"):
    path = userdata / name
    if path.is_file():
        files.append(path)

print(f"Packing {len(files)} files from {userdata} into {output} ...")
with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_STORED) as z:
    for path in files:
        z.write(path, path.relative_to(userdata).as_posix())
temp.replace(output)
size_mb = output.stat().st_size / (1024 * 1024)
print(f"\nDone: {output} ({size_mb:.1f} MB)")
print("Copy this file and the OpenBeach .apk to your Android device (e.g. Downloads), install the .apk,")
print("tap 'Import data package (.zip)' and choose openbeach-data.zip. Afterwards you can delete the .zip.")
PY
