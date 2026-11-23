#pragma once

// The full-screen picture (data/screens/<key>.tga) the game drew last: the menus' GuiTga and the loading screen
// (DrawFullscreenTga). The Android presenter shows that screen's wide version beside a narrower frame (sdl_present.cpp).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace obl::ui::screen_backdrop {

// "data\\screens\\MainMenu.tga" or "mainmenu.tga" -> "mainmenu".
inline std::string KeyOf(std::string_view path)
{
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string_view::npos) path.remove_prefix(slash + 1);
    const size_t dot = path.rfind('.');
    if (dot != std::string_view::npos) path = path.substr(0, dot);
    std::string key(path);
    for (char& c : key)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return key;
}

inline std::string g_key;

// Set by the Android presenter: a screen's part of its wide version, `width` x `height` 16-bit pixels, which the image loader
// shows instead of the original screen (image.cpp), and how far its large scenario picture moves; false when the screen has none
// or it cannot be decoded.
using WideScreen = bool (*)(
        const std::string& key, bool rgb555, int width, int height, std::vector<uint16_t>& pixels, int& pictureX, int& pictureY);
inline WideScreen g_wideScreen = nullptr;

// The screen whose wide version is shown with a moved scenario picture, and the move.
inline std::string g_pictureKey;
inline int g_pictureX = 0, g_pictureY = 0;

inline void SetPictureShift(const std::string& key, int x, int y)
{
    g_pictureKey = key;
    g_pictureX = x;
    g_pictureY = y;
}

// The large scenario picture's move on the screen on show (SCENARIO_PIC_LARGE, gui_variable_image.cpp).
inline void PictureShift(int& x, int& y)
{
    const bool moved = !g_pictureKey.empty() && g_pictureKey == g_key;
    x = moved ? g_pictureX : 0;
    y = moved ? g_pictureY : 0;
}

inline void Shown(std::string_view path)
{
    g_key = KeyOf(path);
}

inline const std::string& Current()
{
    return g_key;
}

} // namespace obl::ui::screen_backdrop
