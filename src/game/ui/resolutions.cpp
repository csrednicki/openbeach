// More in-game resolutions (windowed mode).
//
// The Options > Video combo box ("ResolutionList") is filled by the combo box's populate step from
// k_ResolutionTable: 4 entries {int modeIndex; const char* textKey; int height;} (640x480 ..
// 1280x960), shown only if the renderer's display-mode flag [modeIndex] is set, and selected by
// height in the list's OnItemSelected -> GuiManager::RequestResolution -> the game's resolution change, which
// handles any size in windowed mode. We point the populate loop at our own table (monitor-sized
// entries, all using the always-present 800x600 mode flag), map selections back to width/height,
// provide the texts for the new keys. src/platform/window_placement.cpp measures the monitor and places the window.

#include "core/text16.h"
#include "game/ui/resolutions.h"
#include "game/ui/bar_slide.h"

#include "game/game/game.h"
#include "game/game/game_accessors.h"
#include "game/game/settings_store.h"
#include "game/ui/display_geometry.h"
#include "core/log.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace obl::ui {

namespace {

using TableEntry = ResolutionListEntry;

struct Resolution {
    int width, height;
    std::string key; // "1920_1080"
    Text16 text; // L"1920x1080"
};

std::vector<Resolution> g_resolutions;
int g_screenWidth, g_screenHeight; // BuildAspectResolutionTable's screen; 0 with the desktop list
constexpr int kWideStartHeight = 480, kFourThreeStartHeight = 600; // SetWideScreen's choice for an unlisted resolution
// The 4:3 list a phone gets with Wide screen off: the original game's sizes.
constexpr ui::DisplaySize kFourThree[] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 960}};
std::vector<TableEntry> g_table;
int g_lastScreenHeight;

// The four modes the options list offers before the extended table is built.
constexpr ResolutionListEntry kBaseResolutions[] = {
        {0, "640_480", 480},
        {1, "800_600", 600},
        {2, "1024_768", 768},
        {3, "1280_960", 960},
};

// The list's entries, ordered by width and then height.
void FillTable(std::vector<ui::DisplaySize> sizes)
{
    g_resolutions.clear();
    g_table.clear();
    std::sort(sizes.begin(), sizes.end(), [](ui::DisplaySize a, ui::DisplaySize b) { return a.cx != b.cx ? a.cx < b.cx : a.cy < b.cy; });
    for (ui::DisplaySize s : sizes) {
        Resolution r{s.cx, s.cy, std::to_string(s.cx) + "_" + std::to_string(s.cy), text16::Decimal(s.cx) + u"x" + text16::Decimal(s.cy)};
        g_resolutions.push_back(std::move(r));
    }
    g_table.reserve(g_resolutions.size() + 1);
    for (const auto& r : g_resolutions) g_table.push_back({1, r.key.c_str(), r.height});
    g_table.push_back({}); // a zero entry ends the list, as in k_ResolutionTable
}

void LogTable()
{
    std::string list;
    for (const auto& r : g_resolutions) list += r.key + " ";
    OBL_LOG("resolutions: %s", list.c_str());
}

void BuildTable(int monitorW, int monitorH)
{
    static const ui::DisplaySize kCandidates[] = {
            {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 960}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

    std::vector<ui::DisplaySize> sizes;
    for (ui::DisplaySize s : kCandidates) {
        if (s.cx <= monitorW && s.cy <= monitorH) sizes.push_back(s);
    }
    // Native size (borderless). The list selects by height, so it replaces a same-height entry.
    std::erase_if(sizes, [&](ui::DisplaySize s) { return s.cy == monitorH; });
    sizes.push_back({monitorW, monitorH});
    FillTable(std::move(sizes));
}

const Resolution* FindByHeight(int height)
{
    for (const auto& r : g_resolutions) {
        if (r.height == height) return &r;
    }
    return nullptr;
}

} // namespace

bool IsListedResolutionWidth(int width)
{
    for (const auto& r : g_resolutions) {
        if (r.width == width) return true;
    }
    return false;
}

void BuildResolutionTable(int monitorWidth, int monitorHeight)
{
    BuildTable(monitorWidth, monitorHeight);
    LogTable();
}

void BuildAspectResolutionTable(int screenWidth, int screenHeight)
{
    std::vector<ui::DisplaySize> sizes;
    for (int height : {360, 480, 600, 768}) sizes.push_back({AspectWidth(height, screenWidth, screenHeight), height});
    FillTable(std::move(sizes));
    LogTable();
    g_screenWidth = screenWidth;
    g_screenHeight = screenHeight;
}

bool WideScreenAvailable()
{
    return g_screenWidth > 0 && g_screenHeight > 0;
}

void SetWideScreen(bool wide, int& width, int& height)
{
    if (!WideScreenAvailable()) return;
    if (wide) {
        BuildAspectResolutionTable(g_screenWidth, g_screenHeight);
    } else {
        FillTable({std::begin(kFourThree), std::end(kFourThree)});
        LogTable();
    }
    std::vector<ui::DisplaySize> listed;
    for (const auto& r : g_resolutions) listed.push_back({r.width, r.height});
    const ui::DisplaySize chosen =
            ListedResolution(listed.data(), listed.size(), {width, height}, wide ? kWideStartHeight : kFourThreeStartHeight);
    width = chosen.cx;
    height = chosen.cy;
}

const Char16* ResolutionText(const char* key)
{
    for (const auto& r : g_resolutions) {
        if (key == r.key.c_str()) return r.text.c_str();
    }
    return nullptr;
}

// GuiManager::SyncBarSurfaces (gui_manager_frame.cpp) runs this first. It is run by every GuiManager::LoadScript (the GUI is reloaded
// after each resolution change) and resizes the bottom-bar surface to (screen width x bar height); only when that size changed does it
// recreate the GUI surfaces and move the bar origin to `screen height - bar height`. The game 4:3 modes always change the width,
// so a height-only change (1280x960 -> 1280x720) left the origin below the new screen and the next frame drew the bar past the end of
// the back buffer (heap corruption). Invalidating the surface's recorded width when the screen height changed makes the sync take its
// full rebuild path.
void PrepareBarSurfaceSync(void* gui)
{
    ui::Renderer* renderer = game::GetRenderer();
    int height = renderer->m_height;
    if (g_lastScreenHeight && height != g_lastScreenHeight) {
        ui::SurfaceAt(*renderer, static_cast<int*>(gui)[0x2b])->m_width = 0; // never matches, forces the rebuild
    }
    g_lastScreenHeight = height;
}

std::span<const ResolutionListEntry> ResolutionListEntries()
{
    if (!g_table.empty()) return {g_table.data(), g_table.size() - 1};
    return kBaseResolutions;
}

namespace {
bool g_centeredBar;
int g_barSide; // the goals' panel beside the centred bottom panel (SetBarSide)

// The sandbox has no goals (the progress bar stays hidden, gui_tooltip.cpp): the panel is centred on its own there.
int GoalsSide()
{
    const game::Game* game = game::GetGame();
    return game && game->m_sandbox ? 0 : g_barSide;
}
BarSlide g_barSlide;

uint32_t NowMs()
{
    return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
} // namespace

void SetCenteredBar(bool centered)
{
    g_centeredBar = centered;
}

bool CenteredBar()
{
    return g_centeredBar;
}

void WakeBar()
{
    if (g_centeredBar) g_barSlide.Wake(NowMs());
}

int BarHiddenRows(int barHeight)
{
    if (!g_centeredBar) return 0;
    if (game::settings_store::Exists()) {
        const game::Settings& settings = game::settings_store::Current();
        g_barSlide.SetIdle(settings.m_barAutoHide ? static_cast<uint32_t>(settings.m_barHideSeconds) * 1000u : UINT32_MAX);
    }
    return g_barSlide.HiddenRows(NowMs(), barHeight);
}

int BarLayoutWidth(int screenWidth)
{
    if (!g_centeredBar) return screenWidth;
    return screenWidth >= 800 ? 800 : screenWidth >= 640 ? 640 : screenWidth;
}

int BarOffsetX(int screenWidth)
{
    const int layout = BarLayoutWidth(screenWidth);
    if (!g_centeredBar) return (screenWidth - layout) / 2;
    // The goals' panel and the bottom panel side by side, centred together; the bottom panel stays on the screen.
    return CentredBarX(screenWidth, layout, GoalsSide());
}

void SetBarSide(int width)
{
    g_barSide = width;
}

int BarSideX(int screenWidth)
{
    return g_centeredBar ? std::max(0, BarOffsetX(screenWidth) - GoalsSide()) : 0;
}

bool FindExtendedResolution(int height, int* width)
{
    const Resolution* r = FindByHeight(height);
    if (r) *width = r->width;
    return r != nullptr;
}

} // namespace obl::ui
