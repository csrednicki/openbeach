# Android build (arm64)

An experimental APK of the windowed OpenBeach for Android phones and tablets (`arm64-v8a`, Android 8.0 / API 26 or newer,
targeting API 35 with 16 KB page-size support). Built by `.github/workflows/release.yml` as
`openbeach-<version>-android-arm64.apk` ([releases.md](releases.md#android)).

## Build
```bash
pip install cmake ninja pillow
python tools/obl_android.py                                  # build/android-arm64/OpenBeach.apk
python tools/obl_android.py --run --serial 192.168.1.10:5555 # also install it with adb and start it
```
It needs the Android SDK (`ANDROID_HOME`, else Android Studio's default folder) with an NDK, build-tools and a platform, and a
JDK (`JAVA_HOME`). No Gradle: `src/host_linux/CMakeLists.txt` builds the game with the NDK's toolchain as `libmain.so` (SDL3
static inside, `c++_static`, `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`), then `tools/obl_android.py` compiles the resources
(`aapt2`), SDL's Java activity from SDL's FetchContent tree together with `tools/android/java/` (`javac`, `d8`), bundles the
L0 pack and licences in `assets/`, aligns (`zipalign`) and signs (`apksigner`). The version comes from `--version`,
`OBL_RELEASE_VERSION`, or `version.txt` (`1.12.5` -> `versionCode` `11205`).

### Release signing
Pass `--keystore` / `--key-alias` or set `ANDROID_KEYSTORE_PATH`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and
`ANDROID_KEY_PASSWORD` (`release.yml` decodes `ANDROID_KEYSTORE_BASE64` from repository secrets). Without a release key,
`tools/obl_android.py` signs with `~/.android/debug.keystore` (created on the first build). Keep one release keystore across
releases so in-place APK upgrades preserve imported game data and saves.

## Bringing your own copy to Android
The converter runs on PC. After importing your copy on Windows, Linux or macOS:
1. Run `game\ExportForAndroid.cmd` (Windows), `game/export-android.sh` (Linux) or
   `/Applications/OpenBeach.app/Contents/Resources/export-android.sh` (macOS) to pack `userdata/` into a fast uncompressed
   `openbeach-data.zip`.
2. Copy `openbeach-data.zip` and `openbeach-<version>-android-arm64.apk` to your device (for example `Downloads/`).
3. Install and open OpenBeach, tap **Import data package (.zip)** and choose `openbeach-data.zip`. It unpacks into the app's
   internal storage (`files/userdata/`), preserving any existing `SaveGames/`. Once finished, you can delete the `.zip` file.

Power users can also copy `userdata/` directly over USB or `adb push` into
`/storage/emulated/0/Android/data/org.openbeach.game/files/` (or `files/userdata/`). Long-press the OpenBeach app icon and
choose **Manage data** to re-import, launch the L0 demo or export `openbeach.log`.

## What differs from the Linux build
| Part | Behaviour |
|---|---|
| Start | `LauncherActivity` starts `OpenBeachActivity` (subclass of SDL's `SDLActivity`) immediately when imported data exists, or shows the import/demo screen. `OpenBeachActivity` loads `libmain.so` and calls `main` in `src/launcher/android_main.cpp` (no desktop settings window, no `obl-convert` old-save pass; logs mirror to Android `logcat` under tag `OpenBeach`). |
| Demo | The L0 pack ships in APK `assets/l0/packs/l0/` and unpacks once per installed version into `files/l0/` (`--pack l0`). |
| Screen | Fullscreen landscape (`sensorLandscape`, `SDL_ORIENTATIONS`, cutout `shortEdges`, `window_placement.cpp`). SDL's GLES renderer letterboxes with linear filtering (`SDL_SCALEMODE_LINEAR`) and recreates its texture on `SDL_EVENT_RENDER_DEVICE_RESET`. |
| Touch | The browser's gestures (`kTouchGestures`, `kAbsoluteDrag`, [browser-build.md](../architecture/browser-build.md)) with display-scale thresholds (`TouchGestures::SetScale`) and physical-pixel coordinates (`SDL_GetWindowSizeInPixels`): tap = left click, one-finger drag = camera pan, hold = left drag, two-finger tap = right click, two-finger drag = wheel. |
| Keys | System back button or gesture maps to `Escape` (`input_mapping.cpp`, `SDL_HINT_ANDROID_TRAP_BACK_BUTTON`). Two-finger drag turns a building being placed. |
| Instance lock | No-op on Android (`instance_lock.cpp`): Android enforces one app process and `/tmp` is not writable. |

Not yet done: on-device `.iso` conversion, Google Play `.aab` packaging, x86-64 and 32-bit ABIs.
