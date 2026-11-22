// The handheld's controls on a touch screen (platform/touch_pad.h): the virtual D-pad and the touchpad.

#include "unit_test.h"

#include "platform/touch_pad.h"

using namespace obl::platform;

namespace {

// A 2400x1080 phone: the pad's centre at (229.5, 850.5), radius 183.6.
TouchPad MakePad()
{
    TouchPad pad;
    pad.SetPad(DpadPlacement(2400, 1080));
    return pad;
}

} // namespace

OBL_TEST_CASE("touch_pad.dpad_arrows_by_direction")
{
    const DpadCircle pad = DpadPlacement(2400, 1080);
    OBL_CHECK(DpadArrows(pad, pad.x, pad.y) == 0u); // the middle holds nothing
    OBL_CHECK(DpadArrows(pad, pad.x - 100, pad.y) == kDpadLeft);
    OBL_CHECK(DpadArrows(pad, pad.x, pad.y - 100) == kDpadUp);
    OBL_CHECK(DpadArrows(pad, pad.x + 100, pad.y + 100) == (kDpadRight | kDpadDown)); // a diagonal holds two
    OBL_CHECK(DpadArrows(pad, pad.x + 1000, pad.y + 10) == kDpadRight); // a thumb that slid off keeps its arrow
}

OBL_TEST_CASE("touch_pad.dpad_finger_holds_until_up")
{
    TouchPad pad = MakePad();
    const DpadCircle circle = DpadPlacement(2400, 1080);
    pad.FingerDown(1, circle.x - 150, circle.y, 0);
    OBL_CHECK(pad.Arrows() == kDpadLeft);
    pad.FingerMove(1, circle.x, circle.y + 150); // slides to the down arrow
    OBL_CHECK(pad.Arrows() == kDpadDown);
    pad.FingerUp(1, circle.x, circle.y + 150, 50);
    OBL_CHECK(pad.Arrows() == 0u);
    const TouchPad::Output out = pad.Take();
    OBL_CHECK(out.taps == 0 && out.dx == 0 && out.dy == 0); // the pad's finger is no tap and moves no cursor
}

OBL_TEST_CASE("touch_pad.tap_is_left_click")
{
    TouchPad pad = MakePad();
    pad.FingerDown(7, 1500, 500, 1000);
    pad.FingerMove(7, 1504, 503); // within the slop
    pad.FingerUp(7, 1504, 503, 1100);
    const TouchPad::Output out = pad.Take();
    OBL_CHECK(out.taps == 1);
    OBL_CHECK(out.dx == 0 && out.dy == 0); // the jitter does not move the cursor away from what the tap clicks
    pad.FingerDown(8, 1500, 500, 2000);
    pad.FingerUp(8, 1500, 500, 2000 + TouchPad::kTapMs + 1); // held too long: no click
    OBL_CHECK(pad.Take().taps == 0);
}

OBL_TEST_CASE("touch_pad.drag_moves_cursor")
{
    TouchPad pad = MakePad();
    pad.FingerDown(3, 1000, 500, 0);
    pad.FingerMove(3, 1100, 520); // past the slop: all the motion so far counts
    pad.FingerUp(3, 1100, 520, 50);
    const TouchPad::Output out = pad.Take();
    OBL_CHECK(out.dx == 100 * TouchPad::kGain && out.dy == 20 * TouchPad::kGain);
    OBL_CHECK(out.taps == 0); // a moved finger is no tap
}

OBL_TEST_CASE("touch_pad.two_fingers_are_right_click")
{
    TouchPad pad = MakePad();
    pad.FingerDown(1, 1000, 500, 0);
    pad.FingerDown(2, 1200, 500, 20);
    pad.FingerMove(1, 1300, 600); // two fingers move no cursor
    pad.FingerUp(2, 1200, 500, 80);
    pad.FingerUp(1, 1300, 600, 90);
    const TouchPad::Output out = pad.Take();
    OBL_CHECK(out.twoFingerTaps == 1 && out.taps == 0 && out.dx == 0);
}

OBL_TEST_CASE("touch_pad.dpad_and_cursor_together")
{
    TouchPad pad = MakePad();
    const DpadCircle circle = DpadPlacement(2400, 1080);
    pad.FingerDown(1, circle.x, circle.y - 150, 0); // the left thumb scrolls up
    pad.FingerDown(2, 1500, 500, 10); // the right one taps: a single pointer finger, not two
    pad.FingerUp(2, 1500, 500, 60);
    const TouchPad::Output out = pad.Take();
    OBL_CHECK(pad.Arrows() == kDpadUp);
    OBL_CHECK(out.taps == 1 && out.twoFingerTaps == 0);
}

OBL_TEST_CASE("touch_pad.magnifier_zooms_and_drags")
{
    TouchPad pad = MakePad();
    pad.SetPicture(480, 0, 1440, 1080); // a 4:3 picture on the 2400x1080 screen
    const DpadCircle button = MagnifierButton(DpadPlacement(2400, 1080));
    float x = 0, y = 0;
    OBL_CHECK(!pad.Zoom(x, y));
    pad.FingerDown(1, button.x, button.y, 0);
    pad.FingerMove(1, button.x, button.y + button.radius * 3); // slides onto the D-pad: still the button's, no arrow
    OBL_CHECK(pad.Magnifier() && pad.Arrows() == 0u);
    pad.FingerUp(1, button.x, button.y, 50);
    OBL_CHECK(pad.Zoom(x, y) && x == 1200 && y == 540); // zoomed at once, from the picture's centre
    pad.FingerDown(2, 1300, 500, 100); // a drag to the left moves the view right, half as far in the picture
    pad.FingerMove(2, 1100, 560);
    OBL_CHECK(pad.Zoom(x, y) && x == 1300 && y == 510);
    pad.FingerMove(2, -1000, 560); // the view stays on the picture
    pad.FingerUp(2, -1000, 560, 150);
    OBL_CHECK(pad.Zoom(x, y) && x == 1560 && y == 510);
    pad.FingerDown(3, 1000, 500, 200); // a tap and two fingers neither click nor move the cursor
    pad.FingerUp(3, 1000, 500, 220);
    pad.FingerDown(4, 1000, 500, 300);
    pad.FingerDown(5, 1500, 500, 320);
    pad.FingerUp(5, 1500, 500, 330);
    pad.FingerUp(4, 1000, 500, 340);
    TouchPad::Output out = pad.Take();
    OBL_CHECK(out.taps == 0 && out.twoFingerTaps == 0 && out.dx == 0 && out.dy == 0);
    pad.FingerDown(6, button.x, button.y, 400); // off: the zoom ends and the touchpad is back
    pad.FingerUp(6, button.x, button.y, 410);
    OBL_CHECK(!pad.Zoom(x, y));
    pad.FingerDown(7, 1500, 500, 500);
    pad.FingerUp(7, 1500, 500, 550);
    OBL_CHECK(!pad.Magnifier() && pad.Take().taps == 1);
}
