#pragma once

// The pointer drawn where the system shows none: the handheld's panel (frame_share.cpp) and the Android pad build
// (sdl_present.cpp). X outline, . fill, 12 x 17 game pixels.

namespace obl::platform {

constexpr const char* kArrow[17] = {
        "X           ",
        "XX          ",
        "X.X         ",
        "X..X        ",
        "X...X       ",
        "X....X      ",
        "X.....X     ",
        "X......X    ",
        "X.......X   ",
        "X........X  ",
        "X.....XXXXX ",
        "X..X..X     ",
        "X.X X..X    ",
        "XX  X..X    ",
        "X    X..X   ",
        "     X..X   ",
        "      XX    ",
};

} // namespace obl::platform
