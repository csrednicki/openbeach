#pragma once

#include <algorithm>
#include <climits>
#include <cstdlib>

// Touch screens (own_controls::TouchScreen): a finger that lands beside a small control, on a round icon's see-through corner or
// in a gap of its picture still works it. When a press starts where no control answers, the cursor the GUI sees moves to the
// nearest point within reach where one does, and stays moved by as much until the buttons are up, so that the press and its
// release reach the same control. A press on a control is left alone, so a neighbour never takes it.
namespace obl::ui {
struct GuiManager;
}

namespace obl::gui_touch_snap {

inline constexpr int kReach = 12; // game pixels: about 25 screen pixels on a phone at twice the game's size
inline constexpr int kStep = 2;

// The offset from (x, y) to the nearest point, ring by ring kStep apart up to `reach`, where hit(x, y) holds: (0, 0) when (x, y)
// itself does. False when none does.
template<class Hit> bool Nearest(int x, int y, int reach, Hit hit, int& dx, int& dy)
{
    dx = dy = 0;
    if (hit(x, y)) return true;
    for (int ring = kStep; ring <= reach; ring += kStep) {
        int best = INT_MAX;
        for (int oy = -ring; oy <= ring; oy += kStep)
            for (int ox = -ring; ox <= ring; ox += kStep) {
                if (std::max(std::abs(ox), std::abs(oy)) != ring) continue; // the ring's edge only
                const int distance = ox * ox + oy * oy;
                if (distance < best && hit(x + ox, y + oy)) {
                    best = distance;
                    dx = ox;
                    dy = oy;
                }
            }
        if (best != INT_MAX) return true;
    }
    return false;
}

// The offset one press keeps until both buttons are up.
struct Latch {
    bool active = false;
    int dx = 0, dy = 0;
};

// One frame of the cursor (x, y) with the buttons' states (input_state: 1 pressed this frame, 2 held, 0 up): a press that starts
// finds its offset (find(x, y, dx, dy), false for none), which moves the cursor until the frame both buttons are up in, that frame
// included (the release lands there). True on the frame a press starts.
template<class Find> bool Apply(Latch& latch, int& x, int& y, int left, int right, Find find)
{
    bool starts = false;
    if (!latch.active && (left == 1 || right == 1)) {
        latch.active = starts = true;
        if (!find(x, y, latch.dx, latch.dy)) latch.dx = latch.dy = 0;
    }
    if (!latch.active) return false;
    x += latch.dx;
    y += latch.dy;
    if (left == 0 && right == 0) latch.active = false;
    return starts;
}

// Apply over the manager's active dialogs and bottom panel, when the host is a touch screen: true when a press starts there (the
// GUI then lets the controls see the finger arrive before the press, GuiManager::UpdateMouse).
bool Snap(ui::GuiManager* manager, int& x, int& y, int left, int right);

// The reach in game pixels: kReach, half of it while the magnifier shows the picture twice the size (the platform sets it).
void SetReach(int reach);

// Whether a press at (x, y) would reach a slider (through the snap above): a finger landing there grabs it and drags it at once.
bool SliderNear(ui::GuiManager* manager, int x, int y);

// The front active dialog's rectangle (where a sliding one is going) and identity; false without one.
bool FrontDialog(ui::GuiManager* manager, int rect[4], const void*& id);

} // namespace obl::gui_touch_snap
