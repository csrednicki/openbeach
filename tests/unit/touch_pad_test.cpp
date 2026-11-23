// The handheld's controls on a touch screen (platform/touch_pad.h): the virtual D-pad and the touchpad.

#include "unit_test.h"

#include "platform/touch_pad.h"

#include <cmath>

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

OBL_TEST_CASE("touch_pad.a_still_hold_is_right_click")
{
    TouchPad pad = MakePad();
    pad.FingerDown(1, 1000, 500, 1000);
    pad.Tick(1000 + TouchPad::kHoldMs - 1);
    OBL_CHECK(pad.Take().holds == 0);
    pad.Tick(1000 + TouchPad::kHoldMs);
    pad.Tick(3000); // once a touch
    pad.FingerUp(1, 1000, 500, 3000);
    TouchPad::Output out = pad.Take();
    OBL_CHECK(out.holds == 1 && out.taps == 0 && out.dx == 0);
    pad.FingerDown(2, 1000, 500, 4000); // moved first: no hold
    pad.FingerMove(2, 1100, 500);
    pad.Tick(5000);
    pad.FingerUp(2, 1100, 500, 5000);
    OBL_CHECK(pad.Take().holds == 0);
    pad.FingerDown(3, 1000, 500, 6000); // two fingers tapped: no click at all
    pad.FingerDown(4, 1200, 500, 6020);
    pad.FingerUp(4, 1200, 500, 6080);
    pad.FingerUp(3, 1000, 500, 6090);
    out = pad.Take();
    OBL_CHECK(out.holds == 0 && out.taps == 0);
}

OBL_TEST_CASE("touch_pad.two_finger_swipe_is_a_speed_step")
{
    TouchPad pad = MakePad();
    pad.FingerDown(1, 1000, 400, 0); // one above the other
    pad.FingerDown(2, 1000, 550, 20);
    OBL_CHECK(pad.Take().holds == 0); // two down: nothing yet, they may swipe
    pad.FingerMove(1, 1050, 400);
    pad.FingerMove(2, 1050, 550); // 50 to the right: not yet
    OBL_CHECK(pad.Take().speedSteps == 0);
    pad.FingerMove(1, 1100, 405);
    pad.FingerMove(2, 1100, 555); // 100: one step faster
    pad.FingerMove(1, 1400, 405);
    pad.FingerMove(2, 1400, 555); // and no more in the same touch
    pad.FingerUp(1, 1400, 405, 80);
    pad.FingerUp(2, 1400, 555, 90);
    TouchPad::Output out = pad.Take();
    OBL_CHECK(out.speedSteps == 1 && out.holds == 0 && out.taps == 0 && out.dx == 0);
    pad.FingerDown(3, 1000, 400, 1000); // to the left: slower
    pad.FingerDown(4, 1000, 550, 1010);
    pad.FingerMove(3, 880, 400);
    pad.FingerMove(4, 880, 550);
    pad.FingerUp(3, 880, 400, 1080);
    pad.FingerUp(4, 880, 550, 1090);
    out = pad.Take();
    OBL_CHECK(out.speedSteps == -1 && out.holds == 0);
}

OBL_TEST_CASE("touch_pad.two_fingers_drag_the_zoom")
{
    TouchPad pad = MakePad();
    pad.SetPicture(480, 0, 1440, 1080);
    pad.ToggleMagnifier();
    pad.FingerDown(1, 1300, 500, 0);
    pad.FingerDown(2, 1500, 500, 10);
    pad.FingerMove(1, 1100, 560); // the view follows, half as far on the picture
    float x = 0, y = 0;
    OBL_CHECK(pad.Zoom(x, y) && x == 1300 && y == 510);
    pad.CentreZoom(480, 0); // on a window in the corner: the view stays on the picture
    OBL_CHECK(pad.Zoom(x, y) && x == 840 && y == 270);
    TwoFingerPan pan;
    float dx = 0, dy = 0;
    pan.Down(1, 100, 100);
    pan.Move(1, 140, 100, dx, dy);
    OBL_CHECK(dx == 0 && dy == 0); // one finger: no pan
    pan.Down(2, 300, 100);
    pan.Move(2, 300, 160, dx, dy);
    OBL_CHECK(dx == 0 && dy == 30); // the centre of two moved half the finger's motion
    pan.Up(2);
    pan.Move(1, 200, 100, dx, dy);
    OBL_CHECK(dx == 0 && dy == 0);
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
    OBL_CHECK(out.taps == 1 && out.holds == 0);
}

// A touchpad finger down while the magnifier's button is touched leaves the swipe's fingers, so its lift is not waited for and the
// next two-finger swipe still makes its speed step.
OBL_TEST_CASE("touch_pad.the_magnifier_button_ends_the_fingers_on_the_touchpad")
{
    TouchPad pad = MakePad();
    pad.SetPicture(480, 0, 1440, 1080);
    const DpadCircle button = MagnifierButton(DpadPlacement(2400, 1080));
    pad.FingerDown(1, 1000, 400, 0); // on the touchpad
    pad.FingerDown(2, button.x, button.y, 10); // the magnifier on, and off again
    pad.FingerUp(2, button.x, button.y, 20);
    pad.FingerDown(3, button.x, button.y, 30);
    pad.FingerUp(3, button.x, button.y, 40);
    pad.FingerUp(1, 1000, 400, 50);
    pad.Take();
    pad.FingerDown(4, 1000, 400, 1000);
    pad.FingerDown(5, 1000, 550, 1010);
    pad.FingerMove(4, 1100, 400);
    pad.FingerMove(5, 1100, 550);
    OBL_CHECK(!pad.Magnifier() && pad.Take().speedSteps == 1);
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
    OBL_CHECK(out.taps == 0 && out.holds == 0 && out.dx == 0 && out.dy == 0);
    pad.FingerDown(6, button.x, button.y, 400); // off: the zoom ends and the touchpad is back
    pad.FingerUp(6, button.x, button.y, 410);
    OBL_CHECK(!pad.Zoom(x, y));
    pad.FingerDown(7, 1500, 500, 500);
    pad.FingerUp(7, 1500, 500, 550);
    OBL_CHECK(!pad.Magnifier() && pad.Take().taps == 1);
}

OBL_TEST_CASE("touch_pad.gestures_zoom_through_the_corner_button")
{
    TouchPad pad = MakePad();
    const DpadCircle placed = DpadPlacement(2400, 1080);
    const DpadCircle corner = CornerButton(placed);
    pad.SetButton(corner);
    pad.SetPicture(480, 0, 1440, 1080);
    // The corner button lies inside the D-pad's square, in its lower left.
    OBL_CHECK(corner.x < placed.x && corner.y > placed.y && corner.x - corner.radius >= placed.x - placed.radius - 0.01f);
    OBL_CHECK(pad.OnButton(corner.x, corner.y) && !pad.OnButton(placed.x + placed.radius, placed.y));
    float x = 1300, y = 700;
    pad.ToPicture(x, y);
    OBL_CHECK(x == 1300 && y == 700); // no zoom: unchanged
    pad.ToggleMagnifier();
    x = 1200, y = 540; // the screen's centre shows the picture's centre
    pad.ToPicture(x, y);
    OBL_CHECK(x == 1200 && y == 540);
    x = 1400, y = 740; // 200 pixels off on the zoomed screen are 100 on the picture
    pad.ToPicture(x, y);
    OBL_CHECK(x == 1300 && y == 640);
}

OBL_TEST_CASE("touch_pad.magnifier_off_in_the_options")
{
    TouchPad pad = MakePad();
    pad.SetPicture(480, 0, 1440, 1080);
    const DpadCircle button = MagnifierButton(DpadPlacement(2400, 1080));
    pad.FingerDown(1, button.x, button.y, 0);
    pad.FingerUp(1, button.x, button.y, 50);
    float x = 0, y = 0;
    OBL_CHECK(pad.Zoom(x, y));
    pad.SetMagnifierAllowed(false); // turned off while zoomed: the zoom ends
    OBL_CHECK(!pad.Zoom(x, y) && !pad.MagnifierAllowed());
    pad.FingerDown(2, button.x, button.y, 100); // the button's place is a touchpad finger now: a tap clicks
    pad.FingerUp(2, button.x, button.y, 150);
    OBL_CHECK(!pad.Magnifier() && pad.Take().taps == 1);
}

OBL_TEST_CASE("touch_pad.dpad_stays_off_the_bottom_panel")
{
    const DpadCircle corner = DpadPlacement(2400, 1080);
    // A 4:3 picture leaves a 480-pixel bar on the left: the pad stays in its corner.
    const DpadCircle beside = DpadPlacement(2400, 1080, 480.0f, 1080.0f - 169.2f);
    OBL_CHECK(beside.x == corner.x && beside.y == corner.y);
    // A picture as wide as the screen: the pad stands on the panel's top edge (panel 94 game pixels at 1.8x = 169.2).
    const float barTop = 1080.0f - 169.2f;
    const DpadCircle above = DpadPlacement(2400, 1080, 0.0f, barTop);
    OBL_CHECK(above.x == corner.x && above.radius == corner.radius);
    OBL_CHECK(std::fabs(above.y + above.radius + 0.25f * above.radius - barTop) < 0.01f);
    // The magnifier's button goes with it.
    OBL_CHECK(MagnifierButton(above).y < above.y - above.radius);
}
