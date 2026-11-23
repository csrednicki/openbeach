// Touch gestures: finger events become the mouse actions the game reads (platform/touch_gestures.h).

#include "unit_test.h"

#include "platform/touch_gestures.h"

#include <string>
#include <vector>

using namespace obl::platform;
using Kind = TouchAction::Kind;

namespace {

// The kinds of a list of actions as one line: M move, LD/LU left down/up, RD/RU right down/up, D drag, W wheel.
std::string Kinds(const std::vector<TouchAction>& actions)
{
    std::string line;
    for (const TouchAction& action : actions) {
        if (!line.empty()) line += ' ';
        switch (action.kind) {
        case Kind::Move: line += "M"; break;
        case Kind::LeftDown: line += "LD"; break;
        case Kind::LeftUp: line += "LU"; break;
        case Kind::RightDown: line += "RD"; break;
        case Kind::RightUp: line += "RU"; break;
        case Kind::Drag: line += "D"; break;
        case Kind::Wheel: line += "W"; break;
        case Kind::Zoom: line += action.y > 0 ? "Z+" : "Z-"; break;
        case Kind::Speed: line += action.y > 0 ? "S+" : "S-"; break;
        case Kind::Turn: line += "T"; break;
        }
    }
    return line;
}

float WheelTotal(const std::vector<TouchAction>& actions)
{
    float total = 0;
    for (const TouchAction& action : actions)
        if (action.kind == Kind::Wheel) total += action.y;
    return total;
}

} // namespace

OBL_TEST_CASE("touch.tap_is_a_left_click_at_the_finger")
{
    TouchGestures touch;
    touch.FingerDown(1, 100, 200, 1000);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M");
    OBL_CHECK(touch.AnyFingerDown());
    // A few pixels of wobble stay a tap.
    touch.FingerMove(1, 104, 197, 1050);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerUp(1, 104, 197, 1100);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LD LU");
    for (const TouchAction& action : actions) {
        OBL_CHECK_EQ(action.x, 100.0f); // where the finger went down
        OBL_CHECK_EQ(action.y, 200.0f);
    }
    OBL_CHECK(!touch.AnyFingerDown());
}

OBL_TEST_CASE("touch.one_finger_drag_is_the_camera_drag")
{
    TouchGestures touch;
    touch.FingerDown(1, 100, 100, 0);
    touch.Take();
    touch.FingerMove(1, 100 + TouchGestures::kSlop - 1, 100, 20);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    // Past the slop it becomes a drag from the start, carrying the motion made so far.
    touch.FingerMove(1, 130, 110, 40);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M RD D");
    OBL_CHECK_EQ(actions[1].x, 100.0f);
    OBL_CHECK_EQ(actions[2].x, 30.0f);
    OBL_CHECK_EQ(actions[2].y, 10.0f);
    OBL_CHECK(touch.Panning());
    // Then each step is its own delta.
    touch.FingerMove(1, 125, 130, 60);
    actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "D");
    OBL_CHECK_EQ(actions[0].x, -5.0f);
    OBL_CHECK_EQ(actions[0].y, 20.0f);
    touch.FingerUp(1, 125, 130, 80);
    OBL_CHECK_EQ(Kinds(touch.Take()), "RU");
    OBL_CHECK(!touch.Panning());
    OBL_CHECK(!touch.AnyFingerDown());
}

OBL_TEST_CASE("touch.a_long_press_holds_the_left_button")
{
    TouchGestures touch;
    touch.FingerDown(1, 50, 60, 1000);
    touch.Take();
    touch.Tick(1000 + TouchGestures::kHoldMs - 1);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.Tick(1000 + TouchGestures::kHoldMs);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LD");
    OBL_CHECK_EQ(actions[1].x, 50.0f);
    // The held finger moves the cursor, however far, and is not a camera drag.
    touch.FingerMove(1, 200, 90, 1600);
    actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M");
    OBL_CHECK_EQ(actions[0].x, 200.0f);
    OBL_CHECK(!touch.Panning());
    touch.FingerUp(1, 210, 95, 1700);
    actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LU"); // up where the finger left
    OBL_CHECK_EQ(actions[1].x, 210.0f);
    OBL_CHECK_EQ(actions[1].y, 95.0f);
    // Time passing later does nothing.
    touch.Tick(5000);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
}

OBL_TEST_CASE("touch.a_moved_finger_is_not_a_hold")
{
    TouchGestures touch;
    touch.FingerDown(1, 0, 0, 0);
    touch.FingerMove(1, 40, 0, 100);
    touch.Take();
    touch.Tick(2000);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
}

OBL_TEST_CASE("touch.two_finger_tap_is_a_right_click")
{
    TouchGestures touch;
    touch.FingerDown(1, 100, 100, 0);
    touch.FingerDown(2, 180, 120, 30);
    touch.Take();
    touch.FingerUp(2, 180, 120, 120);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M RD RU");
    OBL_CHECK_EQ(actions[0].x, 100.0f); // at the first finger
    OBL_CHECK_EQ(actions[0].y, 100.0f);
    // The finger still down does nothing more, and nothing is held.
    touch.FingerMove(1, 300, 300, 200);
    touch.FingerUp(1, 300, 300, 260);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    OBL_CHECK(!touch.AnyFingerDown());
    // The next gesture starts clean.
    touch.FingerDown(3, 10, 10, 1000);
    touch.FingerUp(3, 10, 10, 1050);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M M LD LU");
}

OBL_TEST_CASE("touch.two_fingers_resting_are_not_a_click")
{
    TouchGestures touch;
    touch.FingerDown(1, 100, 100, 0);
    touch.FingerDown(2, 180, 120, 30);
    touch.Take();
    touch.FingerUp(2, 180, 120, 30 + TouchGestures::kTapMs + 1);
    touch.FingerUp(1, 100, 100, 900);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
}

OBL_TEST_CASE("touch.two_finger_drag_is_the_wheel")
{
    TouchGestures touch;
    touch.FingerDown(1, 100, 300, 0);
    touch.FingerDown(2, 180, 300, 10);
    touch.Take();
    // Both fingers up by one notch: the list scrolls down (wheel units are negative).
    touch.FingerMove(1, 100, 300 - TouchGestures::kPixelsPerNotch, 50);
    auto first = touch.Take(); // one finger moved, so the centre moved half
    touch.FingerMove(2, 180, 300 - TouchGestures::kPixelsPerNotch, 60);
    auto second = touch.Take();
    OBL_CHECK_EQ(Kinds(first), "W");
    OBL_CHECK_EQ(Kinds(second), "W");
    const float total = WheelTotal(first) + WheelTotal(second);
    OBL_CHECK(total < -0.99f && total > -1.01f);
    OBL_CHECK(!touch.Panning());
    touch.FingerUp(2, 180, 250, 100);
    touch.FingerUp(1, 100, 250, 110);
    OBL_CHECK_EQ(Kinds(touch.Take()), ""); // no click, no right button
    OBL_CHECK(!touch.AnyFingerDown());
}

OBL_TEST_CASE("touch.two_finger_swipe_is_one_game_speed_step")
{
    TouchGestures touch;
    // One finger above the other, both to the right: one step faster, however far they go.
    touch.FingerDown(1, 300, 300, 0);
    touch.FingerDown(2, 300, 420, 10);
    touch.Take();
    touch.FingerMove(1, 330, 300, 20);
    touch.FingerMove(2, 330, 420, 30); // 30 to the right: sideways, not yet far enough
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerMove(1, 400, 302, 40);
    touch.FingerMove(2, 400, 422, 50); // 100: the step
    OBL_CHECK_EQ(Kinds(touch.Take()), "S+");
    touch.FingerMove(1, 700, 302, 60);
    touch.FingerMove(2, 700, 422, 70);
    touch.FingerUp(1, 700, 302, 80);
    touch.FingerUp(2, 700, 422, 90);
    OBL_CHECK_EQ(Kinds(touch.Take()), ""); // no second step, no right click
    // To the left: one step slower.
    touch.FingerDown(1, 600, 300, 1000);
    touch.FingerDown(2, 600, 420, 1010);
    touch.FingerMove(1, 500, 300, 1020);
    touch.FingerMove(2, 500, 420, 1030);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M S-");
}

OBL_TEST_CASE("touch.pinch_is_the_game_zoom")
{
    TouchGestures touch;
    touch.FingerDown(1, 400, 300, 0);
    touch.FingerDown(2, 600, 300, 10); // 200 apart
    touch.Take();
    touch.FingerMove(1, 380, 300, 20);
    touch.FingerMove(2, 620, 300, 30); // 240: not yet a quarter wider
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerMove(2, 660, 300, 40); // 280: a step closer
    OBL_CHECK_EQ(Kinds(touch.Take()), "Z+");
    touch.FingerMove(1, 300, 300, 50); // wider still, then back in: the same pinch makes no other step
    touch.FingerMove(1, 560, 300, 60);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerUp(2, 660, 300, 70);
    touch.FingerUp(1, 560, 300, 80);
    OBL_CHECK_EQ(Kinds(touch.Take()), ""); // no click on the way out
    // The next pinch is the next step: farther this time.
    touch.FingerDown(1, 300, 300, 1000);
    touch.FingerDown(2, 700, 300, 1010);
    touch.FingerMove(1, 450, 300, 1020);
    touch.FingerMove(2, 550, 300, 1030);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M Z-");
}

// With one finger still, the fingers' centre leaves the slop before the span has grown by a quarter: still a pinch, not a swipe
// (a game speed step) or a scroll.
OBL_TEST_CASE("touch.pinch_with_one_finger_still_is_the_game_zoom")
{
    TouchGestures touch;
    touch.FingerDown(1, 400, 300, 0);
    touch.FingerDown(2, 600, 300, 10); // 200 apart
    touch.Take();
    for (int i = 1; i <= 10; ++i) touch.FingerMove(2, 600.0f + 10.0f * static_cast<float>(i), 300, 10 + i); // sideways to 300 apart
    OBL_CHECK_EQ(Kinds(touch.Take()), "Z+");
    touch.FingerUp(2, 700, 300, 30);
    touch.FingerUp(1, 400, 300, 40);
    // Up and down, narrower: farther.
    touch.FingerDown(1, 400, 200, 1000);
    touch.FingerDown(2, 400, 500, 1010); // 300 apart
    touch.Take();
    for (int i = 1; i <= 10; ++i) touch.FingerMove(2, 400, 500.0f - 10.0f * static_cast<float>(i), 1010 + i); // to 200 apart
    OBL_CHECK_EQ(Kinds(touch.Take()), "Z-");
}

OBL_TEST_CASE("touch.a_third_finger_or_a_second_during_a_drag_changes_nothing")
{
    TouchGestures touch;
    // Three fingers: nothing the game has (some systems keep them for their own gestures).
    touch.FingerDown(1, 10, 10, 0);
    touch.FingerDown(2, 50, 10, 5);
    touch.FingerDown(3, 90, 10, 10);
    touch.Take();
    touch.FingerUp(3, 90, 10, 50);
    touch.FingerUp(2, 50, 10, 60);
    touch.FingerUp(1, 10, 10, 70);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    OBL_CHECK(!touch.AnyFingerDown());
    // A second finger joining a camera drag is ignored, and the drag ends with its own finger.
    touch.FingerDown(1, 100, 100, 1000);
    touch.FingerMove(1, 160, 100, 1020);
    touch.Take();
    touch.FingerDown(2, 300, 300, 1030);
    touch.FingerMove(2, 320, 340, 1040);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerMove(1, 170, 100, 1050);
    OBL_CHECK_EQ(Kinds(touch.Take()), "D");
    touch.FingerUp(2, 320, 340, 1060);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerUp(1, 170, 100, 1070);
    OBL_CHECK_EQ(Kinds(touch.Take()), "RU");
}

OBL_TEST_CASE("touch.cancel_lets_go_of_what_is_held")
{
    TouchGestures touch;
    touch.Cancel();
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerDown(1, 0, 0, 0);
    touch.FingerMove(1, 80, 0, 10);
    touch.Take();
    touch.Cancel();
    OBL_CHECK_EQ(Kinds(touch.Take()), "RU");
    OBL_CHECK(!touch.AnyFingerDown());
    touch.FingerDown(2, 5, 5, 1000);
    touch.Tick(2000);
    touch.Take();
    touch.Cancel();
    OBL_CHECK_EQ(Kinds(touch.Take()), "LU");
    // A finger lifted after a cancel is unknown and ignored.
    touch.FingerUp(2, 5, 5, 2100);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
}

OBL_TEST_CASE("touch.placing_aims_with_the_finger_and_presses_on_lift")
{
    TouchGestures touch;
    touch.SetAim(TouchAim::Confirm);
    touch.FingerDown(1, 100, 100, 0);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M");
    // However far it moves, the finger only carries the preview: no camera drag, no hold, nothing pressed.
    touch.FingerMove(1, 220, 180, 50);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M");
    OBL_CHECK_EQ(actions[0].x, 220.0f);
    touch.Tick(2000);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    OBL_CHECK(!touch.Panning());
    // The press is where it lifted.
    touch.FingerUp(1, 230, 190, 2100);
    actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LD LU");
    for (const TouchAction& action : actions) {
        OBL_CHECK_EQ(action.x, 230.0f);
        OBL_CHECK_EQ(action.y, 190.0f);
    }
    OBL_CHECK(!touch.AnyFingerDown());
}

OBL_TEST_CASE("touch.placing_a_second_finger_turns_instead_of_pressing")
{
    TouchGestures touch;
    touch.SetAim(TouchAim::Confirm);
    touch.FingerDown(1, 100, 300, 0);
    touch.FingerDown(2, 180, 300, 10);
    touch.Take();
    // Two fingers are the wheel (the preview turns), and lifting them places nothing.
    touch.FingerMove(1, 100, 300 - 2 * TouchGestures::kPixelsPerNotch, 50);
    touch.FingerMove(2, 180, 300 - 2 * TouchGestures::kPixelsPerNotch, 60);
    OBL_CHECK(WheelTotal(touch.Take()) < -1.9f);
    touch.FingerUp(2, 180, 200, 100);
    touch.FingerUp(1, 100, 200, 110);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    // The first finger lifting first is the right click as usual (the placing modes do not read it); nothing is pressed on the left.
    touch.FingerDown(3, 10, 10, 1000);
    touch.FingerDown(4, 60, 10, 1020);
    touch.FingerUp(3, 10, 10, 1060);
    touch.FingerUp(4, 60, 10, 1080);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M M RD RU");
}

OBL_TEST_CASE("touch.placing_a_second_finger_tap_turns_and_the_first_finger_places")
{
    TouchGestures touch;
    touch.SetAim(TouchAim::Confirm);
    touch.FingerDown(1, 100, 300, 0);
    touch.FingerMove(1, 120, 310, 100);
    touch.Take();
    // Each quick tap of a second finger turns the preview once; the first finger still aims.
    for (uint32_t ms : {500u, 900u}) {
        touch.FingerDown(2, 300, 300, ms);
        touch.FingerUp(2, 300, 300, ms + 80);
        OBL_CHECK_EQ(Kinds(touch.Take()), "T");
    }
    touch.FingerMove(1, 140, 320, 1100);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M");
    touch.FingerUp(1, 150, 330, 1200);
    const auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LD LU");
    OBL_CHECK_EQ(actions[1].x, 150.0f);
}

OBL_TEST_CASE("touch.painting_holds_the_left_button_while_the_finger_drags")
{
    TouchGestures touch;
    touch.SetAim(TouchAim::Paint);
    touch.FingerDown(1, 40, 50, 0);
    auto actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LD");
    OBL_CHECK_EQ(actions[1].x, 40.0f);
    touch.FingerMove(1, 90, 50, 30);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M");
    // A second finger does not take the paint away.
    touch.FingerDown(2, 300, 300, 40);
    touch.FingerMove(2, 320, 300, 50);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerUp(2, 320, 300, 60);
    touch.FingerUp(1, 120, 60, 70);
    actions = touch.Take();
    OBL_CHECK_EQ(Kinds(actions), "M LU");
    OBL_CHECK_EQ(actions[1].x, 120.0f);
    OBL_CHECK(!touch.AnyFingerDown());
}

OBL_TEST_CASE("touch.a_gesture_keeps_the_aim_it_began_with")
{
    TouchGestures touch;
    touch.SetAim(TouchAim::Confirm);
    touch.FingerDown(1, 10, 10, 0);
    touch.Take();
    // The mode may change under the finger (the GUI leaves the placing mode): this gesture is still an aim.
    touch.SetAim(TouchAim::None);
    touch.FingerMove(1, 200, 10, 20);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M");
    touch.FingerUp(1, 200, 10, 40);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M LD LU");
    // The next one is the map's again: a drag is the camera.
    touch.FingerDown(2, 10, 10, 1000);
    touch.FingerMove(2, 90, 10, 1020);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M M RD D");
    touch.Cancel();
    OBL_CHECK_EQ(Kinds(touch.Take()), "RU");
    // And a cancel lets the painting button go.
    touch.SetAim(TouchAim::Paint);
    touch.FingerDown(3, 5, 5, 2000);
    touch.Take();
    touch.Cancel();
    OBL_CHECK_EQ(Kinds(touch.Take()), "LU");
}

OBL_TEST_CASE("touch.display_scale_scales_slop_and_wheel_notches")
{
    TouchGestures touch;
    touch.SetScale(3.0f);
    // Motion past 1x slop (14 px) but inside 3x slop (42 px) still stays a tap.
    touch.FingerDown(1, 100, 100, 0);
    touch.Take();
    touch.FingerMove(1, 130, 100, 30);
    OBL_CHECK_EQ(Kinds(touch.Take()), "");
    touch.FingerUp(1, 130, 100, 60);
    OBL_CHECK_EQ(Kinds(touch.Take()), "M LD LU");
    // Two-finger wheel requires 3x pixels per notch.
    touch.FingerDown(2, 100, 400, 100);
    touch.FingerDown(3, 200, 400, 110);
    touch.Take();
    touch.FingerMove(2, 100, 400 - 3.0f * TouchGestures::kPixelsPerNotch, 150);
    auto first = touch.Take();
    touch.FingerMove(3, 200, 400 - 3.0f * TouchGestures::kPixelsPerNotch, 160);
    auto second = touch.Take();
    const float total = WheelTotal(first) + WheelTotal(second);
    OBL_CHECK(total < -0.99f && total > -1.01f);
}
