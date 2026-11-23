#include "game/ui/own_sprites.h"

#include "controls_tab_hand.gen.h" // assets/ui/controls_tab_hand.png, cmake/embedded_icon.cmake
#include "core/log.h"
#include "core/png_read.h"
#include "game/ui/own_controls.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <string>
#include <utility>

namespace obl::own_sprites {

namespace {

constexpr const char* kAudioTab = "ui3_options_t_audio";
constexpr const char* kControlsTab = "obl_options_t_controls"; // own_controls.cpp names it in the Controls tab's row

// The options tabs' frame, the box their icon lies in (the Audio tab's loudspeaker) and a column of the flat face left of it.
constexpr int kTabWidth = 44, kTabHeight = 31;
constexpr int kIconLeft = 11, kIconRight = 37, kIconTop = 5, kIconBottom = 26;
constexpr int kFaceX = 10;

// A renderer colour in 5-bit red and blue and 6-bit green, for 555 and 565 alike.
struct Rgb {
    int r, g, b;
};
Rgb Unpack(uint16_t color, bool is555)
{
    if (is555) return {(color >> 10) & 31, ((color >> 5) & 31) * 2, color & 31};
    return {(color >> 11) & 31, (color >> 5) & 63, color & 31};
}
int Luma(Rgb c)
{
    return 2 * c.r + c.g + 2 * c.b;
}

int Nearest(const std::vector<uint16_t>& palette, bool is555, Rgb target)
{
    int best = 0, bestDistance = INT_MAX;
    for (size_t i = 0; i < palette.size(); ++i) {
        const Rgb c = Unpack(palette[i], is555);
        const int distance =
                (c.r - target.r) * (c.r - target.r) + (c.g - target.g) * (c.g - target.g) + (c.b - target.b) * (c.b - target.b);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace

bool ReadHand(std::span<const uint8_t> png, Hand& hand)
{
    png_read::Image image;
    std::string error;
    if (!png_read::Decode(png, image, error) || image.indexed || image.bitDepth != 8 || (image.channels != 2 && image.channels != 4))
        return false;
    hand.width = image.width;
    hand.height = image.height;
    hand.alpha.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height));
    for (size_t i = 0; i < hand.alpha.size(); ++i) hand.alpha[i] = image.bytes[i * image.channels + image.channels - 1];
    return true;
}

bool DrawHandTab(
        std::vector<uint8_t>& page, int width, int height, int frames, const std::vector<uint16_t>& palette, bool is555, const Hand& hand)
{
    if (hand.alpha.size() != static_cast<size_t>(hand.width) * static_cast<size_t>(hand.height)) return false;
    if (width != kTabWidth || height != kTabHeight || frames <= 0 || palette.size() < 256 ||
            page.size() != static_cast<size_t>(width) * frames * height * 2)
        return false;
    const size_t stride = static_cast<size_t>(width) * frames; // pixels a row
    auto pixel = [&](int frame, int x, int y) {
        return &page[(static_cast<size_t>(y) * stride + static_cast<size_t>(frame) * width + x) * 2];
    };
    for (int f = 0; f < frames; ++f) {
        int left = kIconRight, right = -1, top = kIconBottom, bottom = -1, white = -1, whiteLuma = -1;
        for (int y = kIconTop; y < kIconBottom; ++y) {
            const uint8_t* face = pixel(f, kFaceX, y);
            for (int x = kIconLeft; x < kIconRight; ++x) {
                uint8_t* p = pixel(f, x, y);
                if (face[1] == 0 || p[1] == 0) return false; // the face is opaque where the icon is
                if (p[0] == face[0]) continue;
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                const int luma = Luma(Unpack(palette[p[0]], is555));
                if (luma > whiteLuma) {
                    whiteLuma = luma;
                    white = p[0];
                }
                p[0] = face[0];
            }
        }
        if (white < 0) return false; // no icon to replace
        const int x0 = (left + right + 1) / 2 - hand.width / 2, y0 = std::max(0, (top + bottom + 1) / 2 - hand.height / 2);
        const Rgb face = Unpack(palette[pixel(f, kFaceX, y0)[0]], is555), shine = Unpack(palette[white], is555);
        const int half = Nearest(palette, is555, {(face.r + shine.r) / 2, (face.g + shine.g) / 2, (face.b + shine.b) / 2});
        for (int y = 0; y < hand.height; ++y)
            for (int x = 0; x < hand.width; ++x) {
                const uint8_t alpha = hand.alpha[static_cast<size_t>(y) * hand.width + x];
                const int px = x0 + x, py = y0 + y;
                if (alpha < kHalfAlpha || px < 0 || px >= width || py < 0 || py >= height) continue;
                pixel(f, px, py)[0] = static_cast<uint8_t>(alpha >= kSolidAlpha ? white : half);
            }
    }
    return true;
}

std::vector<sprite_pack::Sprite> Make(const std::filesystem::path& spritesDir, bool is555)
{
    std::vector<sprite_pack::Sprite> sprites;
    if (!own_controls::TouchScreen()) return sprites;
    sprite_pack::Sprite tab;
    std::string error;
    if (!sprite_pack::ReadSprite(spritesDir, kAudioTab, is555, tab, error) || tab.anims.size() != 1 || tab.palettes.empty()) return sprites;
    sprite_pack::Anim& anim = tab.anims[0];
    std::vector<uint8_t> page;
    // The tab's frames lie in one row of one sheet.
    if (anim.uniqueFrames != anim.columns || !sprite_pack::ReadPage(spritesDir, anim, 0, page, error)) return sprites;
    Hand hand;
    if (!ReadHand(kControlsTabHand, hand)) {
        OBL_LOG("own sprites: the Controls tab's hand (assets/ui/controls_tab_hand.png) does not decode");
        return sprites;
    }
    if (!DrawHandTab(page, anim.width, anim.height, anim.columns, tab.palettes[0], is555, hand)) {
        OBL_LOG("own sprites: %s is not the tab the Controls tab is drawn from", kAudioTab);
        return sprites;
    }
    anim.page = std::move(page);
    anim.sheets.clear();
    anim.name = kControlsTab;
    tab.name = kControlsTab;
    sprites.push_back(std::move(tab));
    return sprites;
}

} // namespace obl::own_sprites
