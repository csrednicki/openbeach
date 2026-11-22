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

## Test build with the handheld's controls
`python tools/obl_android.py --pad` (`-DOBL_ANDROID_PAD=ON`) builds `build/android-arm64-pad/OpenBeach-pad.apk`, the same app
with the controls of the [handheld build](handheld.md) on the touch screen instead of the gestures above (`touch_pad.h`, unit tests
`touch_pad.*`). It has the same package name and installs over the touch build, keeping the data.

| Finger | Does |
|---|---|
| On the D-pad (lower left, in the bar beside a 4:3 picture) | Holds the arrow keys while it stays down: they scroll the map, like the handheld's D-pad. Its direction picks the arrows, two between them; a thumb that slides off keeps its direction. |
| Anywhere else, moved | A touchpad: the cursor follows the finger's motion (x1.5) and stays where it was left, on the picture. |
| Tap | Left click where the cursor is. |
| A second finger | Right click; while a building or scenery is placed, Space instead (turns it, the handheld's X). |
| On the magnifier button (above the D-pad) | Turns the magnifier on or off (the button turns blue). Turning it on shows the picture twice the size at once, from its centre; a finger then drags the zoomed view (it stays on the picture) and neither clicks nor moves the cursor, while the D-pad keeps scrolling. Turning it off ends the zoom. |

The presenter draws the D-pad and the arrow cursor over the frame (`sdl_present.cpp`), as Android shows no pointer for fingers.
The Video tab of the in-game options gets the handheld's two sliders (`own_controls::SetPadPointer`): `[Video] CursorSpeed` scales
the touchpad (the default 30 is x1.5, 10 is x0.5) and `[Video] ScrollSpeed` the map's scrolling by the D-pad and the screen edge.
Not in this build: holding the left button (painting paths by dragging) and the camera drag with the right one.

Not yet done: on-device `.iso` conversion, Google Play `.aab` packaging, x86-64 and 32-bit ABIs.
