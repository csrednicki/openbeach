// The centred bottom panel's slide (game/ui/bar_slide.h).

#include "unit_test.h"

#include "game/ui/bar_slide.h"

#include <vector>

using obl::ui::BarSlide;

OBL_TEST_CASE("bar_slide.shown_then_slides_out_when_idle")
{
    BarSlide slide;
    OBL_CHECK(slide.HiddenRows(1000, 94) == 0); // shown at first
    OBL_CHECK(slide.HiddenRows(1000 + BarSlide::kIdleMs - 1, 94) == 0);
    OBL_CHECK(slide.HiddenRows(1000 + BarSlide::kIdleMs + BarSlide::kSlideMs / 2, 94) == 47); // half way down
    OBL_CHECK(slide.HiddenRows(1000 + BarSlide::kIdleMs + BarSlide::kSlideMs, 94) == 94);
    OBL_CHECK(slide.HiddenRows(1000 + 60000, 94) == 94); // stays out of sight
}

OBL_TEST_CASE("bar_slide.action_brings_it_back")
{
    BarSlide slide;
    slide.HiddenRows(0, 94);
    const uint32_t gone = BarSlide::kIdleMs + BarSlide::kSlideMs;
    OBL_CHECK(slide.HiddenRows(gone, 94) == 94);
    slide.Wake(gone);
    OBL_CHECK(slide.HiddenRows(gone, 94) == 94); // slides in from where it was
    OBL_CHECK(slide.HiddenRows(gone + BarSlide::kSlideMs / 2, 94) == 47);
    OBL_CHECK(slide.HiddenRows(gone + BarSlide::kSlideMs, 94) == 0);
    OBL_CHECK(slide.HiddenRows(gone + BarSlide::kIdleMs - 1, 94) == 0); // and stays for the idle time after the action
}

OBL_TEST_CASE("bar_slide.strip_moves_panel_over_saved_map")
{
    // A 4x4 screen (pitch 5): the panel is the bottom 3 rows of columns 1..2; the map under it is 9, the panel rows 1, 2, 3.
    std::vector<uint16_t> screen(5 * 4, 9);
    std::vector<uint16_t> map;
    obl::ui::SaveStrip(screen.data(), 5, 1, 1, 2, 3, map);
    for (int y = 1; y < 4; ++y)
        for (int x = 1; x < 3; ++x) screen[static_cast<size_t>(y) * 5 + x] = static_cast<uint16_t>(y);
    obl::ui::SlideStrip(screen.data(), 5, 1, 1, 2, 3, 2, map); // two rows hidden
    OBL_CHECK(screen[1 * 5 + 1] == 9 && screen[2 * 5 + 2] == 9); // the map is back above
    OBL_CHECK(screen[3 * 5 + 1] == 1 && screen[3 * 5 + 2] == 1); // the panel's first row is at the bottom
    OBL_CHECK(screen[3 * 5 + 0] == 9 && screen[3 * 5 + 3] == 9); // columns beside the panel are untouched
}

OBL_TEST_CASE("bar_slide.goals_and_panel_centred_together")
{
    // 2608x1200 at 1304x600: the 103 wide goals and the 800 wide panel take 903 of 1304, from 200.
    OBL_CHECK(obl::ui::CentredBarX(1304, 800, 103) == 303);
    OBL_CHECK(obl::ui::CentredBarX(782, 640, 103) == 122); // 782x360 with the 640 panel
    OBL_CHECK(obl::ui::CentredBarX(1304, 800, 0) == 252); // no goals: the panel alone in the middle
    OBL_CHECK(obl::ui::CentredBarX(840, 800, 103) == 40); // too narrow for both: the panel stays on the screen
}

OBL_TEST_CASE("bar_slide.goals_beside_the_panel_only_where_they_fit")
{
    OBL_CHECK(obl::ui::GoalsFitBeside(1044, 800, 103)); // wide: beside the panel
    OBL_CHECK(obl::ui::GoalsFitBeside(782, 640, 103));
    OBL_CHECK(!obl::ui::GoalsFitBeside(800, 800, 103)); // 4:3 at 800x600: their original place
    OBL_CHECK(!obl::ui::GoalsFitBeside(640, 640, 103));
    OBL_CHECK(!obl::ui::GoalsFitBeside(1304, 800, 0)); // no goals (the sandbox)
}
