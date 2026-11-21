# Android build (arm64)

An APK of the windowed OpenBeach for Android phones and tablets (arm64-v8a, Android 9 / API 28 or newer). Tried on an Android 16
phone (Snapdragon, Adreno). Experimental: not part of CI or the releases.

## Build
```
pip install cmake ninja
python tools/obl_android.py                                  # build/android-arm64/OpenBeach.apk
python tools/obl_android.py --run --serial 192.168.1.10:5555 # also install it with adb and start it
```
It needs the Android SDK (`ANDROID_HOME`, else Android Studio's default folder) with an NDK, build-tools and a platform, and a
JDK (`JAVA_HOME`). No Gradle: `src/host_linux/CMakeLists.txt` builds the game with the NDK's toolchain as `libmain.so` (SDL3
static inside, `c++_static`), then the script compiles SDL's Java activity from SDL's source tree together with
`tools/android/java/` (javac, d8), links `tools/android/AndroidManifest.xml`, the icon and the L0 pack as assets (aapt2) and
signs the APK with the debug key in `~/.android/debug.keystore` (made on the first build). The version comes from `version.txt`.

## What differs from the Linux build
| Part | Behaviour |
|---|---|
| Start | `OpenBeachActivity` extends SDL's `SDLActivity`; SDL loads `libmain.so` and calls `main` of `remaster_main.cpp`, which runs the game in this process as on Linux. The activity passes `--game-dir` and `--no-settings`. |
| Game directory | The app's external files directory, `/storage/emulated/0/Android/data/org.openbeach.game/files/`, which a USB cable or `adb push` reaches. The L0 demo comes in the APK's assets and is copied to `l0/packs/l0/` there on the first start of each installed version; without `packs/base/pack.json` the demo plays in `l0/` (`OBL_PACK=l0`), as `launch.sh` does on handhelds. For the full game copy the contents of `userdata/` of an import (`packs/base/`, `remaster/`, `BeachLife.ini`) into `files/`. The log is `openbeach.log` in the game directory. |
| Screen | The window is always fullscreen (immersive, `window_placement.cpp`) and landscape (`SDL_ORIENTATIONS`, set by the activity: SDL would otherwise pick the orientation from the window's size). SDL's GLES renderer presents the game letterboxed. |
| Touch | The browser's gestures (`kTouchGestures`, [browser-build.md](../architecture/browser-build.md)): a tap is a left click, one finger drags the map, two fingers tap a right click or scroll. |
| Keys | The system's back button or gesture is Esc (leaves a mode, opens the menu; `input_mapping.cpp`, `SDL_HINT_ANDROID_TRAP_BACK_BUTTON`). Space has no key: a second finger turns a building being placed. |

Not yet done: importing a copy on the phone (the converter does not run on Android; convert on a PC), a release key and
store packaging, x86-64 and 32-bit ABIs.
