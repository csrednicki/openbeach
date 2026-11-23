// OpenBeach's own sprites made from the pack's art (game/ui/own_sprites.h).

#include "unit_test.h"

#include "game/ui/own_sprites.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

constexpr int kWidth = 44, kHeight = 31, kFrames = 3;
constexpr uint8_t kFace = 1, kWhite = 2, kHalf = 3, kEdge = 4;

uint16_t Rgb565(int r, int g, int b)
{
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

// Three made-up tabs: a flat face with an edge colour in the first column and a white square standing in for the loudspeaker.
std::vector<uint8_t> Tabs()
{
    std::vector<uint8_t> page(static_cast<size_t>(kWidth) * kFrames * kHeight * 2);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth * kFrames; ++x) {
            uint8_t* p = &page[(static_cast<size_t>(y) * kWidth * kFrames + x) * 2];
            const int local = x % kWidth;
            p[0] = local == 0 ? kEdge : local >= 16 && local < 30 && y >= 9 && y < 22 ? kWhite : kFace;
            p[1] = 255;
        }
    return page;
}

// A made-up 14 x 16 hand: solid, with a half-transparent rim.
obl::own_sprites::Hand Block()
{
    obl::own_sprites::Hand hand{14, 16, std::vector<uint8_t>(14 * 16, 255)};
    for (int y = 0; y < hand.height; ++y)
        for (int x = 0; x < hand.width; ++x)
            if (x == 0 || y == 0 || x == hand.width - 1 || y == hand.height - 1) hand.alpha[static_cast<size_t>(y) * hand.width + x] = 128;
    return hand;
}

} // namespace

OBL_TEST_CASE("own_sprites.the_hand_asset_decodes")
{
    // The drawing the game builds in (cmake/embedded_icon.cmake), read from the repository here.
    const auto file = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "assets" / "ui" / "controls_tab_hand.png";
    std::ifstream in(file, std::ios::binary);
    const std::vector<uint8_t> png{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    obl::own_sprites::Hand hand;
    OBL_CHECK(obl::own_sprites::ReadHand(png, hand));
    OBL_CHECK(hand.width == 20 && hand.height == 20); // fits the tab's icon box (26 x 21)
    int solid = 0;
    for (uint8_t alpha : hand.alpha) solid += alpha >= obl::own_sprites::kSolidAlpha;
    OBL_CHECK(solid > 100);
}

OBL_TEST_CASE("own_sprites.hand_replaces_the_tab_icon")
{
    std::vector<uint16_t> palette(256, 0);
    palette[kFace] = Rgb565(10, 20, 31);
    palette[kWhite] = Rgb565(31, 63, 31);
    palette[kHalf] = Rgb565(20, 41, 31);
    palette[kEdge] = Rgb565(5, 5, 5);
    std::vector<uint8_t> page = Tabs();
    OBL_CHECK(obl::own_sprites::DrawHandTab(page, kWidth, kHeight, kFrames, palette, false, Block()));
    for (int f = 0; f < kFrames; ++f) {
        int white = 0, half = 0;
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x) {
                const uint8_t index = page[(static_cast<size_t>(y) * kWidth * kFrames + f * kWidth + x) * 2];
                if (x == 0) {
                    OBL_CHECK(index == kEdge); // outside the icon's box nothing changes
                    continue;
                }
                OBL_CHECK(index == kFace || index == kWhite || index == kHalf);
                white += index == kWhite;
                half += index == kHalf;
            }
        OBL_CHECK(white == 12 * 14 && half == 2 * 14 + 2 * 14); // the hand, in the icon's white with half tones at its rim
        // Centred where the square was (x 16..29, y 9..21): the hand's corner at (16, 7) is its rim, (17, 8) inside it.
        OBL_CHECK(page[(static_cast<size_t>(7) * kWidth * kFrames + f * kWidth + 16) * 2] == kHalf);
        OBL_CHECK(page[(static_cast<size_t>(8) * kWidth * kFrames + f * kWidth + 17) * 2] == kWhite);
    }
}

OBL_TEST_CASE("own_sprites.other_frames_are_refused")
{
    std::vector<uint16_t> palette(256, 0);
    std::vector<uint8_t> page = Tabs();
    OBL_CHECK(!obl::own_sprites::DrawHandTab(page, kWidth - 4, kHeight, kFrames, palette, false, Block())); // not a tab's size
    std::vector<uint8_t> blank(page.size(), 0);
    for (size_t i = 1; i < blank.size(); i += 2) blank[i] = 255;
    OBL_CHECK(!obl::own_sprites::DrawHandTab(blank, kWidth, kHeight, kFrames, palette, false, Block())); // a face without an icon
}
