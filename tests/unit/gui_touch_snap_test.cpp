// A finger beside a small control still works it (game/ui/gui_touch_snap.h).

#include "unit_test.h"

#include "game/ui/gui_touch_snap.h"

using namespace obl::gui_touch_snap;

namespace {

// Two 16x16 controls side by side with no gap, at x 100..115 and 116..131, y 50..65.
bool TwoBoxes(int x, int y)
{
    return x >= 100 && x <= 131 && y >= 50 && y <= 65;
}
int BoxAt(int x)
{
    return x <= 115 ? 0 : 1;
}
bool FindBox(int x, int y, int& dx, int& dy)
{
    return Nearest(x, y, kReach, TwoBoxes, dx, dy);
}

} // namespace

OBL_TEST_CASE("gui_touch_snap.a_hit_stays_where_it_is")
{
    int dx = 7, dy = 7;
    OBL_CHECK(Nearest(117, 60, kReach, TwoBoxes, dx, dy) && dx == 0 && dy == 0); // on the second box, next to the first: kept
    OBL_CHECK(BoxAt(117 + dx) == 1);
}

OBL_TEST_CASE("gui_touch_snap.a_miss_moves_to_the_nearest_control")
{
    int dx = 0, dy = 0;
    OBL_CHECK(Nearest(110, 72, kReach, TwoBoxes, dx, dy)); // 7 below the first box
    OBL_CHECK(dx == 0 && dy == -8 && TwoBoxes(110 + dx, 72 + dy));
    OBL_CHECK(Nearest(140, 58, kReach, TwoBoxes, dx, dy)); // 9 right of the second
    OBL_CHECK(dx == -10 && dy == 0);
    OBL_CHECK(!Nearest(110, 90, kReach, TwoBoxes, dx, dy)); // 25 below: out of reach
}

OBL_TEST_CASE("gui_touch_snap.the_offset_holds_until_release")
{
    Latch latch;
    int x = 110, y = 72;
    OBL_CHECK(Apply(latch, x, y, 1, 0, FindBox)); // the press starts below the box
    OBL_CHECK(x == 110 && y == 64);
    x = 113, y = 74; // the finger wanders while held: the same offset
    OBL_CHECK(!Apply(latch, x, y, 2, 0, FindBox));
    OBL_CHECK(x == 113 && y == 66);
    x = 113, y = 74; // the release frame still gets it
    Apply(latch, x, y, 0, 0, FindBox);
    OBL_CHECK(x == 113 && y == 66 && !latch.active);
    x = 113, y = 74; // with the buttons up the cursor is left alone
    Apply(latch, x, y, 0, 0, FindBox);
    OBL_CHECK(x == 113 && y == 74);
}
