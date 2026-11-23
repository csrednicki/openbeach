#pragma once

// The centred bottom panel's slide (ui::SetCenteredBar, Android): it slides down out of sight after a while without input and
// back up when the player acts again, so the map has the whole screen. No clock of its own: the caller passes milliseconds.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace obl::ui {

class BarSlide {
public:
    static constexpr uint32_t kIdleMs = 4000; // shown this long after the last action, unless SetIdle says otherwise
    static constexpr uint32_t kSlideMs = 250;

    // The Controls options' delay (settings_store); UINT32_MAX keeps the panel in sight.
    void SetIdle(uint32_t ms) { m_idleMs = ms; }

    // An action at `now`: the panel comes back from wherever it is.
    void Wake(uint32_t now)
    {
        const float hidden = HiddenShare(now);
        m_started = true;
        m_lastAction = now;
        m_inStart = now;
        m_inFrom = hidden;
    }

    // The rows of a panel `barHeight` tall that are below the screen at `now`.
    int HiddenRows(uint32_t now, int barHeight)
    {
        if (!m_started) Wake(now); // shown at first
        return static_cast<int>(std::lround(HiddenShare(now) * static_cast<float>(barHeight)));
    }

private:
    // 0 shown .. 1 out of sight.
    float HiddenShare(uint32_t now) const
    {
        if (!m_started) return 0;
        const uint32_t sinceIn = now - m_inStart;
        if (sinceIn < kSlideMs) return m_inFrom * (1.0f - static_cast<float>(sinceIn) / kSlideMs);
        const uint32_t idle = now - m_lastAction;
        if (idle < m_idleMs) return 0;
        return std::min(1.0f, static_cast<float>(idle - m_idleMs) / kSlideMs);
    }

    bool m_started = false;
    uint32_t m_lastAction = 0, m_inStart = 0, m_idleMs = kIdleMs;
    float m_inFrom = 0;
};

// The left edge of a centred panel `layout` wide with the goals' panel (`side` wide) on its left, the two centred together on a
// screen `screenWidth` wide; the panel itself always stays on the screen.
inline int CentredBarX(int screenWidth, int layout, int side)
{
    return std::min(std::max(0, (screenWidth - layout - side) / 2) + side, std::max(0, screenWidth - layout));
}

// A sliding panel is drawn in place with everything on it (its surface, the texts and stars drawn over it, the ticker); then its
// pixels move down by `hidden` rows and the map saved from under it fills the rows above. `pitch` is in pixels.
inline void SaveStrip(const uint16_t* bits, uint32_t pitch, int x, int top, int width, int rows, std::vector<uint16_t>& out)
{
    out.resize(static_cast<size_t>(width) * static_cast<size_t>(rows));
    for (int y = 0; y < rows; ++y)
        std::memcpy(&out[static_cast<size_t>(y) * width], bits + static_cast<size_t>(top + y) * pitch + x, sizeof(uint16_t) * width);
}

inline void SlideStrip(uint16_t* bits, uint32_t pitch, int x, int top, int width, int rows, int hidden, const std::vector<uint16_t>& map)
{
    for (int y = rows - 1; y >= hidden; --y)
        std::memcpy(bits + static_cast<size_t>(top + y) * pitch + x, bits + static_cast<size_t>(top + y - hidden) * pitch + x,
                sizeof(uint16_t) * width);
    for (int y = 0; y < hidden && y < rows; ++y)
        std::memcpy(bits + static_cast<size_t>(top + y) * pitch + x, &map[static_cast<size_t>(y) * width], sizeof(uint16_t) * width);
}

} // namespace obl::ui
