// The wide screens' places, their part under the 800x600 screen and the frame in full colour (platform/wide_screen.h).

#include "unit_test.h"

#include "platform/wide_screen.h"

#include <cstdint>
#include <vector>

using obl::platform::FindWidePlacement;
using obl::platform::From16;
using obl::platform::FullColourFrame;
using obl::platform::SampleWide;
using obl::platform::ShowsWidePart;
using obl::platform::To16;
using obl::platform::WidePlacement;

OBL_TEST_CASE("wide_screen.every_wide_picture_covers_the_screen")
{
    for (const char* key :
            {"credits", "deepred", "extrascreen", "frontscreen", "loading_1", "loading_2", "mainmenu", "scenarioscreen", "touchhelp"}) {
        const WidePlacement* p = FindWidePlacement(key);
        OBL_CHECK(p != nullptr);
        if (!p) continue;
        // 1914x822: wider than the screen on both sides; at most a few rows above and below repeat its edge rows.
        OBL_CHECK(p->x < -250 && p->x + 1914 * p->scale > 1050);
        OBL_CHECK(p->y <= 8 && p->y + 822 * p->scale >= 592);
    }
    OBL_CHECK(FindWidePlacement("completion0") == nullptr);
}

OBL_TEST_CASE("wide_screen.the_part_under_the_screen")
{
    // 40x4: red left half, blue right half; at scale 1 and x -10 the screen's columns 0..9 are red, 10.. blue.
    std::vector<uint8_t> rgba(40 * 4 * 4, 0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 40; ++x) {
            uint8_t* p = &rgba[(static_cast<size_t>(y) * 40 + x) * 4];
            p[x < 20 ? 0 : 2] = 255;
            p[3] = 255;
        }
    const WidePlacement placement{"test", 1.0f, -10.0f, 0.0f};
    std::vector<uint32_t> out(20 * 4);
    SampleWide(rgba.data(), 40, 4, placement, out.data(), 20, 4);
    OBL_CHECK(out[0] == 0xffff0000u && out[9] == 0xffff0000u);
    OBL_CHECK(out[10] == 0xff0000ffu && out[3 * 20 + 19] == 0xff0000ffu);
    OBL_CHECK(To16(out[0], false) == 0xf800 && To16(out[10], false) == 0x001f); // 565
    OBL_CHECK(To16(out[0], true) == 0x7c00 && To16(out[10], true) == 0x001f); // 555
}

OBL_TEST_CASE("wide_screen.the_frame_keeps_full_colour_where_it_shows_the_picture")
{
    // A soft orange the 16 bits cannot hold; the second pixel was drawn over (a button).
    const uint32_t wide[2] = {0xffc87e3du, 0xffc87e3du};
    const uint16_t wide16[2] = {To16(wide[0], false), To16(wide[1], false)};
    const uint16_t frame[2] = {wide16[0], 0x07e0};
    uint32_t out[2] = {};
    FullColourFrame(frame, 2, wide, wide16, false, 2, 1, out);
    OBL_CHECK(out[0] == 0xffc87e3du);
    OBL_CHECK(out[1] == 0xff00ff00u && From16(0x07e0, false) == 0xff00ff00u);
    OBL_CHECK(From16(0xffff, true) == 0xffffffffu && From16(0, true) == 0xff000000u);
}

OBL_TEST_CASE("wide_screen.a_frame_shows_the_screen_or_the_game")
{
    // 8x8: a quarter of the samples (every fourth pixel of every fourth row) on the picture is still the screen.
    const std::vector<uint16_t> wide(64, To16(0xff3080c0u, false));
    std::vector<uint16_t> frame(wide);
    OBL_CHECK(ShowsWidePart(frame.data(), 8, wide.data(), 8, 8));
    for (int i = 0; i < 64; ++i)
        if (i != 0) frame[static_cast<size_t>(i)] = 0x1234; // 3 of the 4 samples drawn over
    OBL_CHECK(ShowsWidePart(frame.data(), 8, wide.data(), 8, 8));
    frame[0] = 0x1234; // none left: the game
    OBL_CHECK(!ShowsWidePart(frame.data(), 8, wide.data(), 8, 8));
}
