#!/usr/bin/env bash
# The Android release APK (docs/build/releases.md, "Android"): builds libmain.so for arm64-v8a with the Android NDK,
# packages and signs the APK without Gradle (tools/obl_android.py), and verifies its structure, ELF headers and signature.
#   tools/ci/android_release.sh [extra cmake -D options...]
# Writes build/dist/openbeach-<version>-android-arm64.apk. OBL_RELEASE_VERSION overrides version.txt; OBL_ANDROID_BUILD the
# build tree (build/android-release). Release signing uses ANDROID_KEYSTORE_PATH, ANDROID_KEYSTORE_PASSWORD,
# ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD (falls back to a debug keystore when unset).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="${OBL_RELEASE_VERSION:-$(tr -d '[:space:]' < "$root/version.txt")}"
version="${version#v}"
build="${OBL_ANDROID_BUILD:-$root/build/android-release}"
dist="${OBL_DIST_DIR:-$root/build/dist}"
apk="$dist/openbeach-$version-android-arm64.apk"

mkdir -p "$dist"
rm -f "$apk"

python3 "$root/tools/obl_android.py" \
    --build-dir "$build" \
    --type RelWithDebInfo \
    --version "$version" \
    --out "$apk" \
    -- "$@"

python3 "$root/tools/ci/android_smoke.py" "$apk" --version "$version"
ls -lh "$apk"
