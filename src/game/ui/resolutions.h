#pragma once

#include "core/char16.h"
#include <span>

namespace obl::ui {

// Extends Options > Video > resolution with widescreen/native sizes (windowed mode). The platform measures the monitor
// (platform/window_placement.h) and places the window; this list only knows sizes.
void BuildResolutionTable(int monitorWidth, int monitorHeight);
// The Android list (window_placement.cpp): the heights 360, 480, 600 and 768, each at the screen's aspect (AspectWidth).
void BuildAspectResolutionTable(int screenWidth, int screenHeight);

// The bottom panel at its own width (800, or 640 below 800), centred, instead of stretched over a wider screen (Android, set by
// window_placement.cpp before the GUI loads). The panel's script lays out at BarLayoutWidth and moves right by BarOffsetX.
void SetCenteredBar(bool centered);
bool CenteredBar();
int BarLayoutWidth(int screenWidth);
int BarOffsetX(int screenWidth);
// The scenario goals' panel (GuiManager::CreateProgressBar, `width` wide) stands right beside a centred bottom panel, on its left;
// the two are centred together. BarSideX is its left edge (BarOffsetX less its width), 0 without a centred panel.
void SetBarSide(int width);
int BarSideX(int screenWidth);
// The centred panel slides out of sight after a while without input and back on any action (bar_slide.h); without a centred
// panel, or with the Controls options' auto-hide off, it never hides. WakeBar is the platform's report of an action.
void WakeBar();
int BarHiddenRows(int barHeight);

// The label of an extended resolution when `key` is one of the list's own key pointers (compared by address, as the GUI
// passes them back), else null. ui::text_table::Lookup asks this before the language table.
const Char16* ResolutionText(const char* key);

struct ResolutionListEntry {
    int modeIndex;
    const char* textKey;
    int height;
};
// The table behind the resolution list.
std::span<const ResolutionListEntry> ResolutionListEntries();
// The width of the extended entry selected by `height`; false when the list has none (the game modes handle it).
bool FindExtendedResolution(int height, int* width);
// ui::renderer::IsModeSupported accepts every width of the extended list.
bool IsListedResolutionWidth(int width);

// GuiManager::SyncBarSurfaces runs this first (see the .cpp for the height-only resolution change it repairs).
void PrepareBarSurfaceSync(void* gui);

} // namespace obl::ui
