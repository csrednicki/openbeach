#pragma once

// The wide (21:9) versions of the full-screen pictures (assets/screens_wide/<key>.avif, Android only): where each one lies over
// the 800x600 screen it replaces, its part under that screen, and the frame shown in full colour. The places were fitted to the
// original screens, so the buttons and the islands the scripts place stay on their pictures; the rest of the wide picture fills
// the bars beside the screen (sdl_present.cpp). No SDL calls, so the unit tests use it.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace obl::platform {

// A wide picture pixel (u, v) lies at the screen's game pixel (x + u * scale, y + v * scale). The scenario screens' large scenario
// picture (SCENARIO_PIC_LARGE) moves by (pictureX, pictureY) game pixels, onto the middle of the wide picture's white card.
struct WidePlacement {
    std::string_view key;
    float scale, x, y;
    int pictureX = 0, pictureY = 0;
};

inline const WidePlacement* FindWidePlacement(std::string_view key)
{
    // Fitted to the original screens (scale, then the offsets; 1914x822 pictures). The scenario screen is fitted on its island map,
    // where the game draws its own island pictures; it leaves a few rows above and below, which repeat the picture's edge rows. Its
    // white card lies elsewhere than in the original, so the scenario picture moves onto its middle.
    static constexpr WidePlacement kPlacements[] = {
            {"credits", 0.7819f, -348.30f, -1.37f},
            {"deepred", 0.7944f, -370.27f, -1.51f},
            {"extrascreen", 0.7579f, -335.84f, 0.0f},
            {"frontscreen", 0.7444f, -293.42f, -0.46f},
            {"loading_1", 0.7319f, -299.45f, -1.32f},
            {"loading_2", 0.7399f, -302.61f, 0.0f},
            {"mainmenu", 0.7784f, -330.95f, 0.0f},
            {"scenarioscreen", 0.7183f, -266.98f, 6.07f, 13, -2},
            // The touch gestures' help (screen_backdrop::g_touchHelp) has no original: it fills the height, centred.
            {"touchhelp", 600.0f / 822.0f, (800.0f - 1914.0f * 600.0f / 822.0f) / 2.0f, 0.0f},
    };
    for (const WidePlacement& placement : kPlacements)
        if (placement.key == key) return &placement;
    return nullptr;
}

// An opaque ARGB8888 colour in the renderer's 16-bit format, and back the way SDL widens 16-bit textures.
inline uint16_t To16(uint32_t argb, bool rgb555)
{
    const uint32_t r = argb >> 16 & 0xff, g = argb >> 8 & 0xff, b = argb & 0xff;
    return rgb555 ? static_cast<uint16_t>((r >> 3) << 10 | (g >> 3) << 5 | b >> 3)
                  : static_cast<uint16_t>((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
}

inline uint32_t From16(uint16_t pixel, bool rgb555)
{
    const uint32_t r5 = rgb555 ? pixel >> 10 & 31 : pixel >> 11 & 31, b5 = pixel & 31;
    const uint32_t g = rgb555 ? (pixel >> 5 & 31) << 3 | (pixel >> 7 & 7) : (pixel >> 5 & 63) << 2 | (pixel >> 9 & 3);
    return 0xff000000u | (r5 << 3 | r5 >> 2) << 16 | g << 8 | (b5 << 3 | b5 >> 2);
}

// The wide picture (RGBA, `width` x `height`) under an `outW` x `outH` screen, filtered linearly, as opaque ARGB8888; beyond the
// picture's edges its edge pixels repeat.
inline void SampleWide(const uint8_t* rgba, int width, int height, const WidePlacement& placement, uint32_t* out, int outW, int outH)
{
    for (int y = 0; y < outH; ++y) {
        const float v = std::clamp((static_cast<float>(y) + 0.5f - placement.y) / placement.scale - 0.5f, 0.0f, height - 1.0f);
        const int v0 = static_cast<int>(v), v1 = std::min(v0 + 1, height - 1);
        const float fv = v - static_cast<float>(v0);
        for (int x = 0; x < outW; ++x) {
            const float u = std::clamp((static_cast<float>(x) + 0.5f - placement.x) / placement.scale - 0.5f, 0.0f, width - 1.0f);
            const int u0 = static_cast<int>(u), u1 = std::min(u0 + 1, width - 1);
            const float fu = u - static_cast<float>(u0);
            uint32_t pixel = 0xff000000u;
            for (int c = 0; c < 3; ++c) {
                auto at = [&](int pu, int pv) { return static_cast<float>(rgba[(static_cast<size_t>(pv) * width + pu) * 4 + c]); };
                const float top = at(u0, v0) + (at(u1, v0) - at(u0, v0)) * fu;
                const float bottom = at(u0, v1) + (at(u1, v1) - at(u0, v1)) * fu;
                pixel |= static_cast<uint32_t>(std::lround(top + (bottom - top) * fv)) << (16 - 8 * c);
            }
            out[static_cast<size_t>(y) * outW + x] = pixel;
        }
    }
}

// Whether the game's 16-bit frame shows the wide picture's part (`wide16`, its To16 pixels, which the game drew): a quarter of
// every fourth row's every fourth pixel at least, so buttons, texts and the loading bar leave it a screen and the game's map does
// not. The 16-bit part is converted once per picture: the presenter asks every frame.
inline bool ShowsWidePart(const uint16_t* frame, int pitchPixels, const uint16_t* wide16, int width, int height)
{
    int samples = 0, same = 0;
    for (int y = 0; y < height; y += 4)
        for (int x = 0; x < width; x += 4) {
            ++samples;
            same += frame[static_cast<size_t>(y) * pitchPixels + x] == wide16[static_cast<size_t>(y) * width + x];
        }
    return same * 4 >= samples;
}

// The game's 16-bit frame in full colour: where it still shows the wide picture's part (`wide16`, the To16 of `wide`) that part's
// own colours, elsewhere (buttons, texts, a fade) the frame's.
inline void FullColourFrame(const uint16_t* frame, int pitchPixels, const uint32_t* wide, const uint16_t* wide16, bool rgb555, int width,
        int height, uint32_t* out)
{
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const size_t at = static_cast<size_t>(y) * width + x;
            const uint16_t pixel = frame[static_cast<size_t>(y) * pitchPixels + x];
            out[at] = pixel == wide16[at] ? wide[at] : From16(pixel, rgb555);
        }
}

} // namespace obl::platform
