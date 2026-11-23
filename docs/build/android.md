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
| Screen | Fullscreen landscape (`sensorLandscape`, `SDL_ORIENTATIONS`, cutout `shortEdges`, `window_placement.cpp`). SDL's GLES renderer letterboxes with linear filtering (`SDL_SCALEMODE_LINEAR`); the screens before a game that have a wide (21:9) version in the APK (`assets/screens_wide/<key>.avif`, decoded by Android's `AImageDecoder` from Android 12) show that version instead of the original: the image loader takes its part under the 800x600 screen as a 16-bit image (`image.cpp` through `ui::screen_backdrop::g_wideScreen`; the renderer draws 16-bit images opaque, `renderer_draw.cpp`), and the presenter draws the whole picture at the same place behind the frame, so it fills the bars, and shows the frame in full colour wherever it still shows that part (`FullColourFrame`; buttons, texts and fades keep the frame's 16 bits) (`DrawWideBackdrop`; the screen is the last full-screen picture drawn, `ui::screen_backdrop`, unit tests `screen_backdrop.*`). Each wide picture's place was fitted to its original screen (`wide_screen.h`, unit tests `wide_screen.*`), so the buttons and islands the scripts place stay on their pictures (the scenario screen on its island map, where the game draws its own island pictures, and its large scenario picture moves onto the middle of the wide picture's white card, `screen_backdrop::PictureShift`; the few rows it leaves above and below repeat its edge rows). They show while the frame still shows that part (`ShowsWidePart`), so also while a scenario loads, and never over the game. Older Androids, the other screens and gameplay keep the original with black bars. The intro movie shows only the 16:9 band in the middle of its 640x480 frame, at the screen's height, and recreates its texture on `SDL_EVENT_RENDER_DEVICE_RESET`. Options > Video lists the heights 360, 480, 600 and 768 at the screen's aspect (2608x1200: 782x360 ... 1670x768), which fill the screen; the activity passes the screen as `OBL_SCREEN_SIZE` (`ui::BuildAspectResolutionTable`, `AspectWidth`). The bottom panel keeps its own width there (800, or the 640 one below 800 wide), centred, and the map runs to the screen's bottom edge beside it (`ui::SetCenteredBar`: the `MainItem` lays out at that width, `gui_script.cpp`; the panel's surface has that width, `gui_manager_frame.h`; no bottom trim, `renderer_helpers.cpp`). The panel slides down out of sight after a delay without input (Options > Controls, 4 s at first) and back up on any key, mouse or finger action (`bar_slide.h`, unit tests `bar_slide.*`); while sliding it is drawn in place with its texts, stars and ticker and its pixels move down over the map saved from under it (`SaveStrip`, `SlideStrip`); while it is not fully shown it takes no clicks. Dialogs open in the middle of the map above the panel whatever the script anchors them to (a building's corner, the build panel's bottom edge, the screen's centre), except those anchored to the right (the build mode's cancel panel); they open there, as if the panel were always shown (`ParseDialogItem`); windows open there every time, not at a position remembered in `[Dialog]` (`ShowDialog`). Options > Video > Wide screen (`[Video] WideScreen`, on at first) keeps that list; off, the list is the original 4:3 one (640x480, 800x600, 1024x768, 1280x960) and the game switches at once (`ui::SetWideScreen`). A saved resolution the list does not hold, and the first start without `BeachLife.ini` (800x600), become its 480-high one when wide (1044x480 on 2608x1200) and 800x600 when not (`ListedResolution` after the settings load, unit test `geometry.an_android_start_plays_wide`); the menus stay 800x600. Options > Video > Frame counter (`[Video] FpsCounter`, off at first) shows the game's frames per second (`GameClock::m_fps`) at the top of the screen, in its middle, clear of rounded corners, in the GUI's title font, white on a see-through dark box (`DrawFrameCounter` at the end of the GUI overlay, `game.cpp`). Both checkboxes stand first under the resolution in the Video tab's one column, whose rows are 23 pixels apart from 100 (`own_controls.cpp`, `SpreadVideoRows`); Wide screen only where the screen's size is known. |
| Touch | Options > Controls > Touch controls (on at first): the browser's gestures (`kTouchGestures`, `kAbsoluteDrag`, [browser-build.md](../architecture/browser-build.md)) with display-scale thresholds (`TouchGestures::SetScale`) and physical-pixel coordinates (`SDL_GetWindowSizeInPixels`): tap = left click, one-finger drag = camera pan, hold = right click, two-finger drag up or down = wheel, two-finger swipe to the right or left = one game speed step faster or slower, pinch = one step of the game's zoom, a finger on a slider drags it. Off: the handheld's controls below. |
| Keys | System back button or gesture maps to `Escape` (`input_mapping.cpp`, `SDL_HINT_ANDROID_TRAP_BACK_BUTTON`). Two-finger drag turns a building being placed. |
| Instance lock | No-op on Android (`instance_lock.cpp`): Android enforces one app process and `/tmp` is not writable. |

## The Controls options
The in-game options get a fourth tab, Controls, on touch screens (`own_controls::SetTouchScreen`, inserted into the script after
the Audio tab like the Video sliders, unit tests `own_controls.*`). Its icon is made at load time from the player's Audio tab: the
loudspeaker is painted over with the tab's face and a touching hand drawn in its white (`own_sprites.cpp`, unit tests
`own_sprites.*`). The hand is OpenBeach's own drawing, `assets/ui/controls_tab_hand.png` (20x20, white: opaque pixels take the
icon's white, half-transparent ones a tone half way to the face), built into the program (`cmake/embedded_icon.cmake`); a pack without that tab (L0) shows the bank's dummy. Saved in `[Controls]` of `BeachLife.ini` (`settings_ini.h`):

| Option | Key | Does |
|---|---|---|
| Touch controls | `TouchControls` (1) | On: the gestures above. Off: the handheld's controls below. Switching drops the fingers, the held arrows and the cursor. |
| Magnifier | `Magnifier` (1) | The magnifier's button: above the D-pad, or with the gestures in the D-pad's lower left corner (`CornerButton`), where it zooms the same way but the fingers keep working the gestures through the zoom: they land where the zoomed screen shows (`TouchPad::ToPicture`), and a drag pans the map, not the zoom. Off hides the button and ends a zoom. The button shows only on the game map (`InGameplay`), not in the menus nor while a world loads. |
| Hide the panel | `BarAutoHide` (1) | The centred bottom panel slides out of sight when idle; off keeps it shown. |
| Hide after | `BarHideSeconds` (4) | The slider's delay, 1 to 60 s, shown beside it (`BAR_HIDE_SECONDS`). |

The rows stand 30 pixels apart, as the Audio tab's; the Video tab's cursor and scroll speed sliders stand here (193 and
223) instead, shown only while the touch controls are off: they are a `Define OblPadOnly` placed before the options dialog and
named by the tab, which `GuiDefine` shows, draws and gives the mouse only while `own_controls::DefineShown` holds, and the Video tab's checkbox rows stand 23 apart in one column under the resolution, its own rows first (`own_controls.cpp`, `SpreadVideoRows`). With the
magnifier on, a window that shows (and the front one when it comes on) gets the zoomed view centred on it (`FrontDialog`),
and two fingers drag the view (`TwoFingerPan` with the gestures; the D-pad's touchpad drags it with one or two).

## Fingers beside small controls
On touch screens a press that starts where no control answers moves the cursor the GUI sees to the nearest point within 12
game pixels (6 under the magnifier) where a button, checkbox, radio button, slider or tab does, and keeps it moved by as much
until the release. A finger inside a control's rectangle keeps to that control, so a small one's see-through pixels do not hand
the press to a bigger neighbour; the zoom buttons come last of all, so the small game speed buttons below them win (a pinch
zooms)
(`gui_touch_snap.h`, `GuiManager::UpdateMouse`, unit tests `gui_touch_snap.*`). The controls' own hit tests decide, so the
see-through pixels of a round icon and the gaps in one are covered too; a press on a control is never moved, so a neighbour
cannot take it. The active dialogs come front to back, a dialog's face hiding the controls below it and a modal one all the others.
A control takes a press only after the cursor hovered it (`gui_controls::ButtonMouse`, the button's state 2): a mouse always
passes over it first, a finger lands at once. So a press that starts on a touch screen first reaches the GUI without buttons at
the same place (`GuiManager::UpdateMouse`, `DispatchButtons`); without it the scenery panel's bulldozer and close button
lit up and did nothing when the finger landed on their see-through pixels or was moved onto them (unit tests
`gui_controls.button_press_needs_a_hover_first`, `UpdateMouse lets a landing finger hover before its press`).

The scenario's goals (`GuiManager::CreateProgressBar`) stand beside the centred bottom panel (not in the sandbox, which has none: the panel is centred alone there), 20 pixels to its left, their top
at the panel's (on the screen's bottom edge where the 640 panel is lower than they are); the two are centred together (`ui::SetBarSide`, `CentredBarX`, unit test `bar_slide.goals_and_panel_centred_together`)
and slide out of sight together (`game.cpp`). A screen without room for both (Wide screen off at 800x600 or 640x480) keeps the
goals in their original place over the map (`ui::GoalsBesideBar`, `GoalsFitBeside`, unit test `bar_slide.goals_beside_the_panel_only_where_they_fit`).

Coming back to the app keeps the cursor off the screen: SDL's mouse state is (0, 0) without a mouse, and it would scroll the map
to its top left (`RestoreSdlMousePosition`).

## The handheld's controls
With Touch controls off, the controls of the [handheld build](handheld.md) work the touch screen instead of the gestures
(`touch_pad.h`, unit tests `touch_pad.*`).

| Finger | Does |
|---|---|
| On the D-pad (lower left, in the bar beside a 4:3 picture) | Holds the arrow keys while it stays down: they scroll the map, like the handheld's D-pad. Its direction picks the arrows, two between them; a thumb that slides off keeps its direction. |
| Anywhere else, moved | A touchpad: the cursor follows the finger's motion (x1.5) and stays where it was left, on the picture. |
| Tap | Left click where the cursor is. |
| Held still (`TouchPad::kHoldMs`) | Right click where the cursor is; while a building or scenery is placed, Space instead (turns it, the handheld's X). |
| Two fingers swiped sideways | One game speed step: to the right faster, to the left slower. MIUI keeps three fingers for its screenshots (it cancels the touch), so the speed is no three-finger tap. |
| On the magnifier button (above the D-pad) | Turns the magnifier on or off (the button turns blue). Turning it on shows the picture twice the size at once, from its centre; a finger then drags the zoomed view (it stays on the picture) and neither clicks nor moves the cursor, while the D-pad keeps scrolling. Turning it off ends the zoom. |

The presenter draws the D-pad and the arrow cursor over the frame (`sdl_present.cpp`), as Android shows no pointer for fingers.
When the picture reaches under the D-pad (a resolution at the screen's aspect), the pad and its magnifier button stand above the
game's bottom panel (`GuiManager::m_barHeight`): they may cover the map, never the panel. Beside a centred panel that leaves the
corner free (1044x480 and wider on a 2608x1200 phone) they stay in the corner.
The Video tab of the in-game options gets the handheld's two sliders (`own_controls::SetPadPointer`): `[Video] CursorSpeed` scales
the touchpad (the default 30 is x1.5, 10 is x0.5) and `[Video] ScrollSpeed` the map's scrolling by the D-pad and the screen edge.
Not with these controls: holding the left button (painting paths by dragging) and the camera drag with the right one.

Not yet done: on-device `.iso` conversion, Google Play `.aab` packaging, x86-64 and 32-bit ABIs.
