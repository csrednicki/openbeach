#include "game/ui/gui_touch_snap.h"

#include "game/ui/gui_action_entries.h"
#include "game/ui/gui_containers_types.h"
#include "game/ui/gui_controls_types.h"
#include "game/ui/gui_manager_frame.h"
#include "game/ui/own_controls.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace obl::gui_touch_snap {

namespace {

using ui::GuiWidget;

Latch g_latch;
int g_reach = kReach;
// While set, only controls whose rectangle holds this point answer (Find).
struct Point {
    int x, y;
};
const Point* g_within;
// While false, the zoom buttons do not answer (Find): the pinch zooms, and the game speed buttons beside them come first.
bool g_zoomButtons = true;

bool ZoomButton(const GuiWidget* w)
{
    const auto action = static_cast<const ui::GuiButton*>(w)->m_leftAction;
    return action == &gui::gui_action_entries::ZoomIn || action == &gui::gui_action_entries::ZoomOut;
}

bool Is(const GuiWidget* w, const char* name)
{
    return std::strcmp(w->ClassName(), name) == 0;
}

bool Inside(const GuiWidget* w, int x, int y)
{
    return x >= w->m_x0 && x <= w->m_x1 && y >= w->m_y0 && y <= w->m_y1;
}

bool Control(GuiWidget* w, int x, int y, bool slidersOnly);

// A widget's own hit test without its side effect: the hover flag the probes would leave on widgets the real dispatch never
// reaches again (it stops at the taker) would keep them lit.
bool Probe(GuiWidget* w, int x, int y)
{
    const uint8_t hovered = w->m_hovered;
    const bool hit = w->HitTest(x, y);
    w->m_hovered = hovered;
    return hit;
}

bool AnyChild(ui::GuiContainerChildNode* sentinel, int x, int y, bool slidersOnly)
{
    if (!sentinel) return false;
    for (ui::GuiContainerChildNode* node = sentinel->m_next; node && node != sentinel; node = node->m_next)
        if (Control(node->m_widget, x, y, slidersOnly)) return true;
    return false;
}

// Whether a control that takes a click answers at (x, y): a button, checkbox, radio button, slider or tab, also inside the
// containers (the current tab's page only, a hidden Define's none). The controls' own hit tests decide, see-through pixels and
// hidden buttons included. `slidersOnly` looks for sliders alone.
bool Control(GuiWidget* w, int x, int y, bool slidersOnly)
{
    if (!w) return false;
    const bool within = !g_within || Inside(w, g_within->x, g_within->y);
    if (Is(w, "GuiSlider")) return within && w->m_visible && Probe(w, x, y);
    if (Is(w, "GuiButton")) return within && !slidersOnly && (g_zoomButtons || !ZoomButton(w)) && Probe(w, x, y);
    if (Is(w, "GuiCheckBox") || Is(w, "GuiRadioButton")) return within && !slidersOnly && w->m_visible && Probe(w, x, y);
    if (Is(w, "GuiDialog")) return AnyChild(static_cast<ui::GuiDialog*>(w)->m_children, x, y, slidersOnly);
    if (Is(w, "GuiMainItem")) return AnyChild(static_cast<ui::GuiMainItem*>(w)->m_children, x, y, slidersOnly);
    if (Is(w, "GuiTab")) return AnyChild(static_cast<ui::GuiTab*>(w)->m_children, x, y, slidersOnly);
    if (Is(w, "GuiRadioGroup")) return AnyChild(static_cast<ui::GuiRadioGroup*>(w)->m_children, x, y, slidersOnly);
    if (Is(w, "GuiDefine")) {
        auto* define = static_cast<ui::GuiDefine*>(w);
        return define->m_shown && AnyChild(define->m_children, x, y, slidersOnly);
    }
    if (Is(w, "GuiTabWindow")) {
        auto* window = static_cast<ui::GuiTabWindow*>(w);
        for (ui::GuiTabNode* tab = window->m_tabs; tab && !slidersOnly; tab = tab->m_next) {
            const GuiWidget* button = tab->m_widget; // the tab's own button
            if (button && Inside(button, x, y) && (!g_within || Inside(button, g_within->x, g_within->y))) return true;
        }
        return Control(window->m_current, x, y, slidersOnly);
    }
    return false;
}

// What the press would reach at (x, y): the active dialogs front to back, a dialog's face hiding what lies below it and a modal
// one everything else, then the bottom panel.
bool Answers(ui::GuiManager* manager, int x, int y, bool slidersOnly = false)
{
    using namespace gui_manager_frame;
    for (Node* node = manager->m_dialogs->m_next; node != manager->m_dialogs; node = node->m_next) {
        if (node->m_entry->m_state != kActiveDialog) continue;
        auto* dialog = static_cast<ui::GuiDialog*>(node->m_entry->m_dialog);
        if (Control(dialog, x, y, slidersOnly)) return true;
        if (dialog->m_modal || Probe(dialog, x, y)) return false;
    }
    return MainItemTakesMouse(manager) && Control(manager->m_mainItem, x, y, slidersOnly);
}

// The offset to the control a press at (x, y) reaches. A finger inside a control's rectangle keeps to that control (its nearest
// answering pixel): a small control's see-through pixels would otherwise hand the press to a bigger neighbour. Elsewhere the
// nearest control within reach. The zoom buttons come last of all: the game speed buttons below them are small, and a pinch zooms.
bool Find(ui::GuiManager* manager, int x, int y, int& dx, int& dy)
{
    const auto hit = [manager](int px, int py) { return Answers(manager, px, py); };
    const Point inside{x, y};
    for (const bool zoomButtons : {false, true}) {
        g_zoomButtons = zoomButtons;
        g_within = &inside;
        bool found = Nearest(x, y, g_reach, hit, dx, dy);
        g_within = nullptr;
        found = found || Nearest(x, y, g_reach, hit, dx, dy);
        if (found) {
            g_zoomButtons = true;
            return true;
        }
    }
    return false;
}

} // namespace

void SetReach(int reach)
{
    g_reach = reach;
}

bool Snap(ui::GuiManager* manager, int& x, int& y, int left, int right)
{
    if (!own_controls::TouchScreen() || !manager) return false;
    return Apply(g_latch, x, y, left, right, [manager](int px, int py, int& dx, int& dy) { return Find(manager, px, py, dx, dy); });
}

bool SliderNear(ui::GuiManager* manager, int x, int y)
{
    if (!manager) return false;
    // Where the snap would put the press, and whether a slider is there (no other control nearer).
    int dx = 0, dy = 0;
    return Find(manager, x, y, dx, dy) && Answers(manager, x + dx, y + dy, true);
}

bool FrontDialog(ui::GuiManager* manager, int rect[4], const void*& id)
{
    using namespace gui_manager_frame;
    id = nullptr;
    if (!manager) return false;
    for (Node* node = manager->m_dialogs->m_next; node != manager->m_dialogs; node = node->m_next) {
        if (node->m_entry->m_state != kActiveDialog) continue;
        const ui::GuiDialogData* dialog = node->m_entry->m_dialog;
        // A sliding window is not where it is going yet: its base rectangle is.
        const bool sliding = dialog->m_animating != 0;
        rect[0] = sliding ? dialog->m_baseX0 : dialog->m_x0;
        rect[1] = sliding ? dialog->m_baseY0 : dialog->m_y0;
        rect[2] = sliding ? dialog->m_baseX1 : dialog->m_x1;
        rect[3] = sliding ? dialog->m_baseY1 : dialog->m_y1;
        id = dialog;
        return true;
    }
    return false;
}

} // namespace obl::gui_touch_snap
