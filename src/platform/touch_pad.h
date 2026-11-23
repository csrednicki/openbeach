#pragma once

// The handheld's controls on a touch screen (Android with the Controls options' touch controls off, docs/build/android.md), without
// SDL calls or game state: a virtual D-pad in the lower left corner holds the arrow keys, which scroll the map, as the handheld's
// D-pad does. Every other finger works a touchpad: its motion moves the cursor, a tap is a left click, a finger held still a right
// click (the backend turns it into Space, which turns the building, while one is being placed) and a two-finger swipe a game
// speed step. A button above the D-pad turns the magnifier on and off (unless the Controls options turn the button off): while it
// is on, the presenter shows the picture twice the size, from its centre, and a finger drags the zoomed view instead of working
// the cursor. Positions are client pixels.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace obl::platform {

// Where the backend may read fingers as a D-pad and a touchpad instead of the gestures (touch_gestures.h): the Controls options
// (settings_store: m_touchControls) choose between them while the game runs.
constexpr bool kTouchPadAvailable =
#ifdef __ANDROID__
        true;
#else
        false;
#endif

enum : unsigned { kDpadLeft = 1, kDpadRight = 2, kDpadUp = 4, kDpadDown = 8 };

struct DpadCircle {
    float x = 0, y = 0, radius = 0;
};

// Sized by the screen's short side and kept in the lower left corner: with a 4:3 game on a phone it lies in the bar beside the
// picture. A bottom panel that reaches under it (its left edge `barLeft`, a game at the screen's aspect) lifts it onto the panel's
// top edge `barTop`: it may cover the map but not the panel.
inline DpadCircle DpadPlacement(
        int width, int height, float barLeft = std::numeric_limits<float>::max(), float barTop = std::numeric_limits<float>::max())
{
    // 2608x1200 with a 4:3 picture leaves a 504-pixel bar: the pad and its margin take 459 of it.
    const float radius = 0.17f * static_cast<float>(std::min(width, height));
    const float margin = 0.25f * radius;
    DpadCircle pad{margin + radius, static_cast<float>(height) - margin - radius, radius};
    if (pad.x + radius > barLeft) pad.y = std::min(pad.y, barTop - margin - radius);
    return pad;
}

// The magnifier's button, above the D-pad.
inline DpadCircle MagnifierButton(const DpadCircle& pad)
{
    const float radius = 0.4f * pad.radius;
    return {pad.x, pad.y - pad.radius - 0.25f * pad.radius - radius, radius};
}

// The magnifier's button with the touch gestures, where there is no D-pad: in the lower left corner of the D-pad's place.
inline DpadCircle CornerButton(const DpadCircle& pad)
{
    const float radius = 0.4f * pad.radius;
    return {pad.x - pad.radius + radius, pad.y + pad.radius - radius, radius};
}

inline bool Inside(const DpadCircle& circle, float x, float y)
{
    const float dx = x - circle.x, dy = y - circle.y;
    return dx * dx + dy * dy <= circle.radius * circle.radius;
}

// The arrows a finger at (x, y) holds, by its direction from the centre: none in the middle, two between two arrows (within 22.5
// degrees of a diagonal). The distance from the centre does not count otherwise, so a thumb that slides off the pad keeps scrolling.
inline unsigned DpadArrows(const DpadCircle& pad, float x, float y)
{
    const float dx = x - pad.x, dy = y - pad.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < 0.2f * pad.radius) return 0;
    const float share = 0.3827f * distance; // sin(22.5 degrees)
    return (dx < -share ? kDpadLeft : 0u) | (dx > share ? kDpadRight : 0u) | (dy < -share ? kDpadUp : 0u) | (dy > share ? kDpadDown : 0u);
}

// The motion of the centre of exactly two fingers (with the gestures, the magnifier's view follows it), in client pixels.
class TwoFingerPan {
public:
    void Down(int64_t id, float x, float y) { m_fingers.push_back({id, x, y}); }
    // The centre's motion when two fingers are down, else (0, 0).
    void Move(int64_t id, float x, float y, float& dx, float& dy)
    {
        dx = dy = 0;
        for (Finger& finger : m_fingers) {
            if (finger.id != id) continue;
            if (m_fingers.size() == 2) {
                dx = (x - finger.x) / 2;
                dy = (y - finger.y) / 2;
            }
            finger.x = x;
            finger.y = y;
        }
    }
    void Up(int64_t id)
    {
        m_fingers.erase(std::remove_if(m_fingers.begin(), m_fingers.end(), [id](const Finger& f) { return f.id == id; }), m_fingers.end());
    }
    void Clear() { m_fingers.clear(); }

private:
    struct Finger {
        int64_t id;
        float x, y;
    };
    std::vector<Finger> m_fingers;
};

class TouchPad {
public:
    // What the fingers did since the last Take.
    struct Output {
        double dx = 0, dy = 0; // cursor motion
        int taps = 0; // left clicks
        int holds = 0; // right clicks: a finger held still kHoldMs (Space while placing)
        int speedSteps = 0; // two-finger swipes: 1 faster (to the right), -1 slower
    };

    static constexpr float kSlop = 10.0f; // density-independent pixels a tap may wander
    static constexpr uint32_t kTapMs = 300;
    static constexpr uint32_t kHoldMs = 450;
    static constexpr double kGain = 1.5; // cursor pixels per finger pixel at the default cursor speed
    static constexpr float kSwipe = 80.0f; // density-independent pixels two fingers go sideways for a speed step

    void SetPad(const DpadCircle& pad)
    {
        m_pad = pad;
        m_button = MagnifierButton(pad);
    }
    const DpadCircle& Pad() const { return m_pad; }
    // After SetPad: the button elsewhere (CornerButton for the gestures).
    void SetButton(const DpadCircle& button) { m_button = button; }
    const DpadCircle& Button() const { return m_button; }
    bool OnButton(float x, float y) const { return m_magnifierAllowed && Inside(m_button, x, y); }
    // The Controls options' magnifier: without it the button is neither drawn nor touched, and a zoom in progress ends.
    void SetMagnifierAllowed(bool allowed)
    {
        m_magnifierAllowed = allowed;
        if (!allowed) m_magnifier = false;
    }
    bool MagnifierAllowed() const { return m_magnifierAllowed; }
    void SetScale(float scale) { m_scale = scale > 1.0f ? scale : 1.0f; }
    void SetGain(double gain) { m_gain = gain; }
    // Where the game's picture lies on the screen: the zoom's centre stays where the half-size view fits on it.
    void SetPicture(float x, float y, float w, float h)
    {
        m_picX = x;
        m_picY = y;
        m_picW = w;
        m_picH = h;
        ClampZoom(); // the game changed its resolution
    }

    void FingerDown(int64_t id, float x, float y, uint32_t ms)
    {
        if (OnButton(x, y)) {
            ToggleMagnifier();
            m_dpad.push_back({id, 0, true}); // held by the button until it is up: it holds no arrow
            return;
        }
        if (Inside(m_pad, x, y)) {
            m_dpad.push_back({id, DpadArrows(m_pad, x, y), false});
            return;
        }
        m_pointers.push_back(id);
        if (m_pointers.size() == 1) {
            m_primary = id;
            m_startX = m_lastX = x;
            m_startY = m_lastY = y;
            m_downMs = ms;
            m_moving = m_twoFingers = m_held = false;
            m_pendingX = m_pendingY = 0;
        } else {
            m_twoFingers = true; // until every finger is up: no motion and no tap
        }
        m_spots.push_back({id, x, y, x, y});
        if (m_pointers.size() == 2)
            for (Spot& spot : m_spots) { // the swipe counts from where both are
                spot.startX = spot.x;
                spot.startY = spot.y;
            }
    }

    void FingerMove(int64_t id, float x, float y)
    {
        for (DpadFinger& finger : m_dpad)
            if (finger.id == id) {
                if (!finger.button) finger.arrows = DpadArrows(m_pad, x, y);
                return;
            }
        for (Spot& spot : m_spots)
            if (spot.id == id) {
                spot.x = x;
                spot.y = y;
            }
        if (m_pointers.size() == 2 && !m_magnifier) SwipeStep();
        if (id != m_primary || (m_twoFingers && !m_magnifier)) return;
        if (m_magnifier) { // one finger or two drag the zoomed view
            // The zoomed picture follows the finger: its screen motion is half as much in the picture.
            m_zoomX -= (x - m_lastX) / 2;
            m_zoomY -= (y - m_lastY) / 2;
            m_lastX = x;
            m_lastY = y;
            ClampZoom();
            return;
        }
        m_pendingX += (x - m_lastX) * m_gain;
        m_pendingY += (y - m_lastY) * m_gain;
        m_lastX = x;
        m_lastY = y;
        // Motion waits until the finger has left the tap's slop, so a tap clicks where the cursor was.
        const float fx = x - m_startX, fy = y - m_startY;
        if (!m_moving && std::sqrt(fx * fx + fy * fy) > kSlop * m_scale) m_moving = true;
        if (!m_moving) return;
        m_out.dx += m_pendingX;
        m_out.dy += m_pendingY;
        m_pendingX = m_pendingY = 0;
    }

    void FingerUp(int64_t id, float x, float y, uint32_t ms)
    {
        const auto dpad = std::find_if(m_dpad.begin(), m_dpad.end(), [id](const DpadFinger& finger) { return finger.id == id; });
        if (dpad != m_dpad.end()) {
            m_dpad.erase(dpad);
            return;
        }
        const auto pointer = std::find(m_pointers.begin(), m_pointers.end(), id);
        if (pointer == m_pointers.end()) return;
        FingerMove(id, x, y);
        m_pointers.erase(pointer);
        std::erase_if(m_spots, [id](const Spot& spot) { return spot.id == id; });
        if (id == m_primary && !m_magnifier && !m_twoFingers && !m_moving && ms - m_downMs <= kTapMs) ++m_out.taps;
        if (m_pointers.empty()) m_swiped = false;
    }

    // The system took the fingers (a gesture of its own, the app in the background).
    void Cancel()
    {
        m_dpad.clear();
        m_pointers.clear();
        m_primary = -1;
        m_spots.clear();
        m_swiped = false;
    }

    // Time passing with the fingers where they are: one finger held still kHoldMs is a right click, once a touch.
    void Tick(uint32_t ms)
    {
        if (m_pointers.size() != 1 || m_pointers[0] != m_primary || m_twoFingers || m_moving || m_held || m_magnifier) return;
        if (ms - m_downMs < kHoldMs) return;
        ++m_out.holds;
        m_held = true;
    }

    bool Magnifier() const { return m_magnifier; }

    // The button's touch: the zoom starts from the picture's centre, or ends.
    void ToggleMagnifier()
    {
        m_magnifier = !m_magnifier;
        m_zoomX = m_picX + m_picW / 2;
        m_zoomY = m_picY + m_picH / 2;
        // A touchpad finger that is down stops working the cursor, and its swipe ends: its lift is not looked for.
        m_pointers.clear();
        m_spots.clear();
        m_swiped = false;
        m_primary = -1;
    }

    // The zoomed view moved by a drag of (dx, dy) on the screen (half as far on the picture), or centred on (x, y): on the picture.
    void PanZoom(float dx, float dy)
    {
        m_zoomX -= dx / 2;
        m_zoomY -= dy / 2;
        ClampZoom();
    }
    void CentreZoom(float x, float y)
    {
        m_zoomX = x;
        m_zoomY = y;
        ClampZoom();
    }

    // A finger on the zoomed screen to where it points on the unzoomed picture, for the gestures (which work through the zoom).
    void ToPicture(float& x, float& y) const
    {
        if (!m_magnifier) return;
        x = m_zoomX + (x - (m_picX + m_picW / 2)) / 2;
        y = m_zoomY + (y - (m_picY + m_picH / 2)) / 2;
    }

    // The point of the picture (client pixels, as without the zoom) that the screen shows twice the size around its centre.
    bool Zoom(float& x, float& y) const
    {
        if (!m_magnifier) return false;
        x = m_zoomX;
        y = m_zoomY;
        return true;
    }

    unsigned Arrows() const
    {
        unsigned arrows = 0;
        for (const DpadFinger& finger : m_dpad) arrows |= finger.arrows;
        return arrows;
    }

    Output Take()
    {
        const Output out = m_out;
        m_out = {};
        return out;
    }

private:
    // Two fingers that went kSwipe sideways (twice as far as up or down): one speed step, once a touch.
    void SwipeStep()
    {
        if (m_swiped || m_spots.size() != 2) return;
        const float dx = (m_spots[0].x - m_spots[0].startX + m_spots[1].x - m_spots[1].startX) / 2;
        const float dy = (m_spots[0].y - m_spots[0].startY + m_spots[1].y - m_spots[1].startY) / 2;
        if (std::fabs(dx) < kSwipe * m_scale || std::fabs(dx) <= 2 * std::fabs(dy)) return;
        m_out.speedSteps += dx > 0 ? 1 : -1;
        m_swiped = true;
    }

    // The zoomed view stays on the picture: its centre at least a quarter of the picture from each edge.
    void ClampZoom()
    {
        m_zoomX = std::clamp(m_zoomX, m_picX + m_picW / 4, m_picX + 3 * m_picW / 4);
        m_zoomY = std::clamp(m_zoomY, m_picY + m_picH / 4, m_picY + 3 * m_picH / 4);
    }

    struct DpadFinger {
        int64_t id;
        unsigned arrows;
        bool button; // the magnifier's button, not the D-pad
    };

    DpadCircle m_pad, m_button;
    float m_scale = 1.0f;
    double m_gain = kGain;
    std::vector<DpadFinger> m_dpad;
    std::vector<int64_t> m_pointers;
    int64_t m_primary = -1;
    float m_startX = 0, m_startY = 0, m_lastX = 0, m_lastY = 0;
    double m_pendingX = 0, m_pendingY = 0;
    uint32_t m_downMs = 0;
    bool m_moving = false, m_twoFingers = false, m_held = false;
    struct Spot {
        int64_t id;
        float x, y, startX, startY;
    };
    std::vector<Spot> m_spots; // the touchpad fingers' positions, for the two-finger swipe
    bool m_swiped = false; // this touch made its speed step
    bool m_magnifier = false, m_magnifierAllowed = true;
    float m_zoomX = 0, m_zoomY = 0;
    float m_picX = 0, m_picY = 0, m_picW = 0, m_picH = 0;
    Output m_out;
};

} // namespace obl::platform
