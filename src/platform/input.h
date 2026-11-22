#pragma once

// The SDL input backend (input.cpp) behind the Input object; its test entry points are declared with the object.
#include "game/ui/input.h"

namespace obl::platform {

// The pointer of the handheld build (a thumb stick, docs/build/handheld.md) and of the Android pad build (a touchpad,
// touch_pad.h) in game-window client pixels; false when nothing moved it yet. The panel thread (frame_share.cpp) or the presenter
// (sdl_present.cpp) draws the cursor there.
bool HandheldCursor(int& x, int& y);
// What the Android pad build's presenter draws (touch_pad.h): the arrows the D-pad holds (kDpadLeft...), whether the magnifier is
// on, and while it zooms the point of the picture it shows twice the size around the picture's centre (client pixels unzoomed).
struct TouchPadView {
    unsigned arrows = 0;
    bool magnifier = false, zoomed = false;
    float zoomX = 0, zoomY = 0;
};
TouchPadView CurrentTouchPadView();

} // namespace obl::platform
