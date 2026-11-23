#pragma once

#include "formats/sprite_pack.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

// OpenBeach's own sprites, made at load time from the player's converted art (which the repository does not hold). Today: the icon of
// the Controls options tab (own_controls.h), the Audio tab with a touching hand drawn over its loudspeaker. The hand is
// OpenBeach's own drawing, assets/ui/controls_tab_hand.png, built into the program.
namespace obl::own_sprites {

// The sprites the bank adds to the pack's; none without a touch screen or when the pack has no Audio tab to start from.
std::vector<sprite_pack::Sprite> Make(const std::filesystem::path& spritesDir, bool is555);

// A drawing for a tab's icon by its opacity alone, row by row: below kHalfAlpha nothing, below kSolidAlpha half way between the
// icon's white and the tab's face, else the white. Draw it white in an image editor, its soft edges half transparent.
struct Hand {
    int width = 0, height = 0;
    std::vector<uint8_t> alpha;
};
inline constexpr uint8_t kHalfAlpha = 64, kSolidAlpha = 192;

// A gray + alpha or RGBA PNG; false for anything else.
bool ReadHand(std::span<const uint8_t> png, Hand& hand);

// Paints the tab's face over the icon of each of `frames` tab frames side by side in `page` (gray + alpha, `width` x `height`
// each) and draws `hand` in the icon's whitest colour, centred where the icon was. `palette` holds the sheet's renderer colours.
// False, with `page` possibly changed, when the frames are not the options tabs' (44 x 31, an icon on a flat face).
bool DrawHandTab(
        std::vector<uint8_t>& page, int width, int height, int frames, const std::vector<uint16_t>& palette, bool is555, const Hand& hand);

} // namespace obl::own_sprites
