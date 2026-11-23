#pragma once

// Touch gestures for the SDL input backend, without SDL calls or game state: finger events in, mouse-like actions out. The game
// is played with a mouse, so a finger is mapped onto what it uses:
//   tap                    left click
//   drag with one finger   camera drag (the right button held, the motion as deltas)
//   press and hold         right click once the finger has stayed still kHoldMs (a slider holds the left button instead)
//   second finger          while placing, its tap or a drag of both fingers turns the preview the first one holds
//   drag with two fingers  the wheel (up and down)
//   swipe with two fingers one game speed step: to the right faster, to the left slower
//   pinch with two fingers one step of the game's zoom (the fingers a quarter wider or narrower)
// A finger that has not moved yet only places the cursor; what it turns into is decided by its motion and by time.
// While the GUI places things (TouchAim) one finger is the cursor instead and the map is not dragged.

#include <cmath>
#include <cstdint>
#include <vector>

namespace obl::platform {

// Where the backend handles fingers itself: the browser, which phones and tablets reach, and Android. The other native hosts keep
// SDL's touch-to-mouse events, so a touch screen there works as before.
constexpr bool kTouchGestures =
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
        true;
#else
        false;
#endif

// What the GUI wants from a finger, set by the backend from the GUI mode (a gesture keeps the aim it began with).
enum class TouchAim {
    None, // the map: a drag moves the camera
    Confirm, // placing a building or scenery: the finger moves the preview, and lifting it is the press (the game places on a press)
    Paint, // painting grass or paving: the left button is down while the finger drags
};

struct TouchAction {
    enum class Kind {
        Move, // the cursor is at (x, y)
        LeftDown, // at (x, y)
        LeftUp,
        RightDown, // at (x, y); the camera drag starts with it
        RightUp,
        Drag, // camera drag motion since the last one: (x, y) are the deltas in client pixels
        Wheel, // x = 0, y = whole notches (a finger moving up scrolls down, like a touch list)
        Zoom, // a pinch step: y = 1 closer (fingers apart), -1 farther
        Speed, // a two-finger swipe: y = 1 faster (to the right), -1 slower
        Turn, // placing: a second finger tapped while the first holds the preview
    };
    Kind kind;
    float x, y;
};

class TouchGestures {
public:
    // Client pixels and milliseconds. `id` tells the fingers apart.
    void FingerDown(int64_t id, float x, float y, uint32_t ms);
    void FingerMove(int64_t id, float x, float y, uint32_t ms);
    void FingerUp(int64_t id, float x, float y, uint32_t ms);
    // What the GUI is doing now; it applies from the next gesture.
    void SetAim(TouchAim aim) { m_nextAim = aim; }
    // Display pixel scale (1 on 1x screens, ~2-3 on high-DPI phones): keeps slop and wheel notches proportional in mm.
    void SetScale(float scale) { m_scale = scale > 1.0f ? scale : 1.0f; }
    // Time passing with the fingers where they are: a finger held still long enough is a right click.
    void Tick(uint32_t ms);
    // The window lost focus or SDL cancelled the touch: whatever is held goes up.
    void Cancel();

    // The actions since the last call, in order.
    std::vector<TouchAction> Take()
    {
        std::vector<TouchAction> out;
        out.swap(m_out);
        return out;
    }
    bool AnyFingerDown() const { return !m_fingers.empty(); }
    // A camera drag by a finger is running: the right button is down on its behalf.
    bool Panning() const { return m_mode == Mode::Pan; }

    // Motion that still counts as a still finger, the time within which a lift is a tap, and the time after which a still finger
    // is a hold; one wheel notch is this many pixels of two-finger motion.
    static constexpr float kSlop = 14.0f;
    static constexpr uint32_t kTapMs = 350;
    static constexpr uint32_t kHoldMs = 450;
    static constexpr float kPixelsPerNotch = 48.0f;
    // A pinch step: the fingers' distance grows or shrinks by this ratio (and at least by the slop). A swipe: two fingers this far
    // sideways (twice as far as up or down when they set off).
    static constexpr float kPinchRatio = 1.25f;
    static constexpr float kSwipePixels = 80.0f;

private:
    enum class Mode {
        Idle,
        One, // one finger down, not decided yet
        Two, // a second finger joined, not decided yet
        Pan, // one finger drags the camera
        Paint, // the left button is held by one finger for painting (TouchAim::Paint)
        Scroll, // two fingers scroll
        Pinch, // two fingers made their zoom step
        Swipe, // two fingers move sideways: one speed step once far enough
        Done, // decided (or too many fingers): nothing more until every finger is up
    };
    struct Finger {
        int64_t id;
        float x, y;
    };

    Finger* Find(int64_t id)
    {
        for (Finger& finger : m_fingers)
            if (finger.id == id) return &finger;
        return nullptr;
    }
    void Emit(TouchAction::Kind kind, float x, float y) { m_out.push_back({kind, x, y}); }
    void Centroid(float& x, float& y) const
    {
        x = y = 0;
        for (const Finger& finger : m_fingers) {
            x += finger.x;
            y += finger.y;
        }
        const float count = static_cast<float>(m_fingers.size());
        x /= count;
        y /= count;
    }
    static float Distance(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }
    float Span() const { return Distance(m_fingers[0].x, m_fingers[0].y, m_fingers[1].x, m_fingers[1].y); }
    // A pinch step once the span left `m_span` by kPinchRatio (and the slop): true when one was made, and the span is the new start.
    bool PinchStep(float span)
    {
        if (std::fabs(span - m_span) <= kSlop * m_scale) return false;
        if (span >= m_span * kPinchRatio)
            Emit(TouchAction::Kind::Zoom, 0, 1);
        else if (span * kPinchRatio <= m_span)
            Emit(TouchAction::Kind::Zoom, 0, -1);
        else
            return false;
        m_span = span;
        return true;
    }
    // A swipe's one step, once the fingers' centre is kSwipePixels to a side of where it set off; the rest of the swipe is nothing.
    void SwipeStep(float cx)
    {
        const float sideways = cx - m_startX;
        if (std::fabs(sideways) < kSwipePixels * m_scale) return;
        Emit(TouchAction::Kind::Speed, 0, sideways > 0 ? 1.0f : -1.0f);
        m_mode = Mode::Done;
    }
    // Whole notches only, as a mouse wheel gives them: the game takes every wheel event as a step (a building being placed turns
    // once per event), so the motion short of a notch waits for more.
    void WheelStep(float notches)
    {
        m_wheel += notches;
        const float whole = std::trunc(m_wheel);
        if (whole == 0) return;
        Emit(TouchAction::Kind::Wheel, 0, whole);
        m_wheel -= whole;
    }
    void Release()
    {
        // Whatever the gesture holds goes up where its finger was.
        if (m_mode == Mode::Pan) Emit(TouchAction::Kind::RightUp, m_lastX, m_lastY);
        if (m_mode == Mode::Paint) Emit(TouchAction::Kind::LeftUp, m_lastX, m_lastY);
    }

    Mode m_mode = Mode::Idle;
    std::vector<Finger> m_fingers; // the fingers down, in the order they came
    std::vector<TouchAction> m_out;
    int64_t m_primary = 0; // the finger of One, Pan and Paint
    float m_startX = 0, m_startY = 0; // where the gesture began (the primary finger; the centroid of Two)
    float m_lastX = 0, m_lastY = 0; // the last position the gesture used
    float m_span = 0; // Two and Pinch: the fingers' distance the next pinch step counts from
    float m_wheel = 0; // Scroll: notches moved but not yet emitted
    uint32_t m_startMs = 0;
    float m_scale = 1.0f;
    TouchAim m_nextAim = TouchAim::None;
    TouchAim m_aim = TouchAim::None; // of the gesture under way
};

inline void TouchGestures::FingerDown(int64_t id, float x, float y, uint32_t ms)
{
    if (Find(id)) return;
    m_fingers.push_back({id, x, y});
    if (m_fingers.size() == 1) {
        m_mode = Mode::One;
        m_aim = m_nextAim;
        m_primary = id;
        m_startX = m_lastX = x;
        m_startY = m_lastY = y;
        m_startMs = ms;
        Emit(TouchAction::Kind::Move, x, y);
        if (m_aim == TouchAim::Paint) {
            m_mode = Mode::Paint;
            Emit(TouchAction::Kind::LeftDown, x, y);
        }
    } else if (m_fingers.size() == 2 && m_mode == Mode::One) {
        m_mode = Mode::Two;
        Centroid(m_startX, m_startY);
        m_lastX = m_startX;
        m_lastY = m_startY;
        m_startMs = ms;
        m_span = Span();
    } else if (m_mode == Mode::One || m_mode == Mode::Two) {
        m_mode = Mode::Done; // three fingers are nothing the game has
    }
    // A finger added to a drag or a hold is ignored: the gesture keeps its finger.
}

inline void TouchGestures::FingerMove(int64_t id, float x, float y, uint32_t ms)
{
    Finger* finger = Find(id);
    if (!finger) return;
    finger->x = x;
    finger->y = y;
    switch (m_mode) {
    case Mode::One:
        if (id != m_primary) break;
        if (m_aim == TouchAim::Confirm) {
            // The preview follows the finger; nothing is pressed until it lifts.
            Emit(TouchAction::Kind::Move, x, y);
            m_lastX = x;
            m_lastY = y;
            break;
        }
        if (Distance(x, y, m_startX, m_startY) <= kSlop * m_scale) break;
        // Moved: a camera drag from where the finger went down, carrying the motion made so far.
        m_mode = Mode::Pan;
        Emit(TouchAction::Kind::Move, m_startX, m_startY);
        Emit(TouchAction::Kind::RightDown, m_startX, m_startY);
        Emit(TouchAction::Kind::Drag, x - m_startX, y - m_startY);
        m_lastX = x;
        m_lastY = y;
        break;
    case Mode::Pan:
        if (id != m_primary) break;
        Emit(TouchAction::Kind::Drag, x - m_lastX, y - m_lastY);
        m_lastX = x;
        m_lastY = y;
        break;
    case Mode::Paint:
        if (id != m_primary) break;
        Emit(TouchAction::Kind::Move, x, y);
        m_lastX = x;
        m_lastY = y;
        break;
    case Mode::Two: {
        float cx, cy;
        Centroid(cx, cy);
        // A pinch changes the fingers' distance well beyond their centre's motion; a two-finger drag moves the centre with it. A
        // pinch waits for its step here: with one finger still, the centre moves half as far as the span and leaves the slop first.
        const float moved = Distance(cx, cy, m_startX, m_startY), span = Span();
        if (std::fabs(span - m_span) > 1.5f * moved) {
            if (PinchStep(span)) m_mode = Mode::Pinch;
            break;
        }
        if (moved <= kSlop * m_scale) break;
        if (std::fabs(cx - m_startX) > 2 * std::fabs(cy - m_startY)) { // sideways: the game speed, not the wheel
            m_mode = Mode::Swipe;
            SwipeStep(cx);
            break;
        }
        m_mode = Mode::Scroll;
        m_wheel = 0;
        WheelStep((cy - m_startY) / (kPixelsPerNotch * m_scale));
        m_lastX = cx;
        m_lastY = cy;
        break;
    }
    case Mode::Scroll: {
        float cx, cy;
        Centroid(cx, cy);
        WheelStep((cy - m_lastY) / (kPixelsPerNotch * m_scale));
        m_lastX = cx;
        m_lastY = cy;
        break;
    }
    case Mode::Pinch: break; // one step a pinch: the fingers must leave before the next
    case Mode::Swipe: {
        float cx, cy;
        Centroid(cx, cy);
        SwipeStep(cx);
        break;
    }
    case Mode::Idle:
    case Mode::Done: break;
    }
    (void)ms;
}

inline void TouchGestures::FingerUp(int64_t id, float x, float y, uint32_t ms)
{
    Finger* finger = Find(id);
    if (!finger) return;
    finger->x = x;
    finger->y = y;
    switch (m_mode) {
    case Mode::One:
        if (id != m_primary) break;
        if (m_aim == TouchAim::Confirm) {
            // The press where the finger left the preview.
            Emit(TouchAction::Kind::Move, x, y);
            Emit(TouchAction::Kind::LeftDown, x, y);
            Emit(TouchAction::Kind::LeftUp, x, y);
            m_mode = Mode::Done;
            break;
        }
        // A finger lifted without having moved, before it became a hold, is a click.
        Emit(TouchAction::Kind::Move, m_startX, m_startY);
        Emit(TouchAction::Kind::LeftDown, m_startX, m_startY);
        Emit(TouchAction::Kind::LeftUp, m_startX, m_startY);
        m_mode = Mode::Done;
        break;
    case Mode::Two:
    case Mode::Scroll:
        // Placing: the second finger turns the preview, by a quick tap or by dragging up or down with the first (the wheel); when
        // it leaves, the first finger goes on aiming and its lift places.
        if (m_aim == TouchAim::Confirm && id != m_primary) {
            if (m_mode == Mode::Two && ms - m_startMs <= kTapMs) Emit(TouchAction::Kind::Turn, x, y);
            m_mode = Mode::One;
            break;
        }
        m_mode = Mode::Done;
        break;
    case Mode::Pan:
    case Mode::Paint:
        if (id != m_primary) break;
        if (m_mode == Mode::Paint) {
            Emit(TouchAction::Kind::Move, x, y); // the button goes up where the finger left
            m_lastX = x;
            m_lastY = y;
        }
        Release();
        m_mode = Mode::Done;
        break;
    case Mode::Pinch:
    case Mode::Swipe: m_mode = Mode::Done; break;
    case Mode::Idle:
    case Mode::Done: break;
    }
    for (size_t i = 0; i < m_fingers.size(); ++i) {
        if (m_fingers[i].id != id) continue;
        m_fingers.erase(m_fingers.begin() + static_cast<std::ptrdiff_t>(i));
        break;
    }
    if (m_fingers.empty()) m_mode = Mode::Idle;
}

inline void TouchGestures::Tick(uint32_t ms)
{
    if (m_mode != Mode::One || m_aim != TouchAim::None || ms - m_startMs < kHoldMs) return;
    Finger* finger = Find(m_primary);
    if (!finger) return;
    // The right click where the finger went down; the rest of the touch is nothing.
    m_mode = Mode::Done;
    Emit(TouchAction::Kind::Move, m_startX, m_startY);
    Emit(TouchAction::Kind::RightDown, m_startX, m_startY);
    Emit(TouchAction::Kind::RightUp, m_startX, m_startY);
}

inline void TouchGestures::Cancel()
{
    Release();
    m_fingers.clear();
    m_mode = Mode::Idle;
}

} // namespace obl::platform
