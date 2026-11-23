#include "game/ui/own_controls.h"

#include "game/game/settings_store.h"
#include "game/ui/resolutions.h"

#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace obl::own_controls {

namespace {

bool g_padPointer;
bool g_touchScreen;

// Below the Video tab's last checkbox ("nudge scrolling", GAME_NUDGE), in the two rows that follow; each slider and its -/+ images
// repeat the Audio tab's volume sliders. The same rows stand in every language's script.
constexpr std::string_view kTab = "TABVideoOptions";
constexpr std::string_view kAnchor = "GAME_NUDGE";
constexpr std::string_view kRows[] = {
        "",
        "            // cursor speed (OpenBeach: the stick of a handheld)",
        "            Text(32, 163, 176, 178, OBL_CURSOR_SPEED, 0, 0, 1)",
        "            Slider(206, 163, 381, 178, CURSOR_SPEED, 0, 1, 0, OBL_TIPTEXT_CURSOR_SPEED)",
        "            Image(194, 163, 207, 178, UI3_slider_minus_l)",
        "            Image(380, 163, 393, 178, UI3_slider_plus_r)",
        "",
        "            // map scroll speed (OpenBeach: the screen edge scrolls fast with a stick-moved cursor)",
        "            Text(32, 178, 176, 193, OBL_SCROLL_SPEED, 0, 0, 1)",
        "            Slider(206, 178, 381, 193, SCROLL_SPEED, 0, 1, 0, OBL_TIPTEXT_SCROLL_SPEED)",
        "            Image(194, 178, 207, 193, UI3_slider_minus_l)",
        "            Image(380, 178, 393, 193, UI3_slider_plus_r)",
};

// The Controls tab, below the Audio tab's button on the options' right edge. Its rows are as far apart as the Audio tab's (30
// pixels, which leaves a finger room); the delay in seconds stands left-aligned right after the slider's plus.
constexpr std::string_view kLastTab = "TABAudioOptions";
constexpr std::string_view kControlsTab[] = {
        "",
        "        // controls tab (OpenBeach: touch screens)",
        "        <TABOblControls(439, 162, 483, 193, OBL_options_t_controls, OBL_TIPTEXT_CONTROLSTAB)>",
        "        {",
        "            // tab title text",
        "            Text(32, 52, 430, 67, OBL_CONTROLS_TITLE, 1, 0, 2)",
        "",
        "            // touch gestures, else a virtual D-pad and a touchpad",
        "            Text(32, 73, 176, 88, OBL_TOUCH_CONTROLS, 0, 0, 1)",
        "            CheckBox(194, 73, 210, 88, UI3_fieldcheckbox, TOUCH_CONTROLS, OBL_TIPTEXT_TOUCH_CONTROLS)",
        "",
        "            // the magnifier's button",
        "            Text(32, 103, 176, 118, OBL_MAGNIFIER, 0, 0, 1)",
        "            CheckBox(194, 103, 210, 118, UI3_fieldcheckbox, MAGNIFIER, OBL_TIPTEXT_MAGNIFIER)",
        "",
        "            // the bottom panel slides out of sight after the delay",
        "            Text(32, 133, 176, 148, OBL_BAR_AUTOHIDE, 0, 0, 1)",
        "            CheckBox(194, 133, 210, 148, UI3_fieldcheckbox, BAR_AUTOHIDE, OBL_TIPTEXT_BAR_AUTOHIDE)",
        "",
        "            Text(32, 163, 176, 178, OBL_BAR_HIDE_DELAY, 0, 0, 1)",
        "            Slider(206, 163, 381, 178, BAR_HIDE_DELAY, 0, 1, 0, OBL_TIPTEXT_BAR_HIDE_DELAY)",
        "            Image(194, 163, 207, 178, UI3_slider_minus_l)",
        "            Image(380, 163, 393, 178, UI3_slider_plus_r)",
        "            Variable(396, 163, 440, 178, BAR_HIDE_SECONDS, 0, 0, 2)",
        "",
        "            // cursor speed (the touchpad) and map scroll speed, with the D-pad and touchpad only",
        "            OblPadOnly",
        "        }",
};

// The speed rows of the Controls tab: a Define the tab names, which shows only while the touch controls are off (DefineShown).
// Here instead of the Video tab, which spreads its rows. Defines stand before the dialogs that name them.
constexpr std::string_view kDialog = "DialogItem InGameOptions";
constexpr std::string_view kPadOnlyName = "OblPadOnly";
constexpr std::string_view kPadOnly[] = {
        "// OpenBeach: the Controls tab's rows for the D-pad and touchpad (own_controls.cpp)",
        "Define OblPadOnly",
        "{",
        "    Text(32, 193, 176, 208, OBL_CURSOR_SPEED, 0, 0, 1)",
        "    Slider(206, 193, 381, 208, CURSOR_SPEED, 0, 1, 0, OBL_TIPTEXT_CURSOR_SPEED)",
        "    Image(194, 193, 207, 208, UI3_slider_minus_l)",
        "    Image(380, 193, 393, 208, UI3_slider_plus_r)",
        "",
        "    Text(32, 223, 176, 238, OBL_SCROLL_SPEED, 0, 0, 1)",
        "    Slider(206, 223, 381, 238, SCROLL_SPEED, 0, 1, 0, OBL_TIPTEXT_SCROLL_SPEED)",
        "    Image(194, 223, 207, 238, UI3_slider_minus_l)",
        "    Image(380, 223, 393, 238, UI3_slider_plus_r)",
        "}",
        "",
};

// The Video tab's checkboxes on touch screens, one column under the resolution: OpenBeach's own rows first (Wide screen where the
// screen's aspect is known, ui::WideScreenAvailable, then the frame counter), then the script's four, which start at 103, 118, 133
// and 148. All stand 23 pixels apart from 100, more room for a finger than 15, and the sixth ends at 230, above the reset button
// (242); the resolution row stays.
constexpr int kVideoFirstRow = 103, kVideoRowCount = 4, kVideoRow = 15;
constexpr int kSpreadFirstRow = 100, kSpreadRow = 23;

struct OwnVideoRow {
    const char* comment;
    const char* text;
    const char* variable;
    const char* tip;
};
constexpr OwnVideoRow kWideScreenRow{
        "resolutions that fill the screen, else the 4:3 ones", "OBL_WIDE_SCREEN", "WIDE_SCREEN", "OBL_TIPTEXT_WIDE_SCREEN"};
constexpr OwnVideoRow kFpsRow{"frames per second at the top of the screen", "OBL_FPS_COUNTER", "FPS_COUNTER", "OBL_TIPTEXT_FPS_COUNTER"};

// The own rows' script lines, in the spread rows from the first.
std::vector<std::string> OwnVideoLines(std::initializer_list<OwnVideoRow> rows)
{
    std::vector<std::string> lines;
    int row = 0;
    for (const OwnVideoRow& r : rows) {
        const std::string y0 = std::to_string(kSpreadFirstRow + row * kSpreadRow),
                          y1 = std::to_string(kSpreadFirstRow + row * kSpreadRow + 15);
        ++row;
        lines.push_back("");
        lines.push_back(std::string("            // ") + r.comment + " (OpenBeach)");
        lines.push_back("            Text(32, " + y0 + ", 176, " + y1 + ", " + r.text + ", 0, 0, 1)");
        lines.push_back("            CheckBox(194, " + y0 + ", 210, " + y1 + ", UI3_fieldcheckbox, " + r.variable + ", " + r.tip + ")");
    }
    return lines;
}

// `rows`, each on a line of its own, inserted before the line break at `lineEnd` (the line keeps its break).
void InsertRows(std::string& script, size_t lineEnd, const std::string_view* rows, size_t count)
{
    const bool crlf = lineEnd > 0 && script[lineEnd - 1] == '\r';
    std::string insert;
    for (size_t i = 0; i < count; ++i) {
        insert += crlf ? "\r\n" : "\n";
        insert += rows[i];
    }
    script.insert(crlf ? lineEnd - 1 : lineEnd, insert);
}

// `rows` after the Video tab's last checkbox line (kAnchor).
void AddVideoRows(std::string& script, const std::string_view* rows, size_t count)
{
    const size_t tab = script.find(kTab);
    if (tab == std::string::npos) return;
    const size_t anchor = script.find(kAnchor, tab);
    if (anchor == std::string::npos) return;
    // The tab ends at its own closing brace: the anchor must come before the next tab opens.
    const size_t nextTab = script.find("<TAB", tab + kTab.size());
    if (nextTab != std::string::npos && nextTab < anchor) return;
    const size_t lineEnd = script.find('\n', anchor);
    if (lineEnd == std::string::npos) return;
    InsertRows(script, lineEnd, rows, count);
}

void AddSpeedSliders(std::string& script)
{
    AddVideoRows(script, kRows, std::size(kRows));
}

// The number at `at` (digits), its value and where it ends.
bool Number(const std::string& script, size_t at, int& value, size_t& end)
{
    end = at;
    while (end < script.size() && script[end] >= '0' && script[end] <= '9') ++end;
    if (end == at) return false;
    value = std::stoi(script.substr(at, end - at));
    return true;
}

// The second and fourth numbers of a `Text(` or `CheckBox(` line inside the Video tab (y0, y1), moved to the spread rows after
// `ownRows` of OpenBeach's.
void SpreadVideoRows(std::string& script, int ownRows)
{
    const size_t tab = script.find(kTab);
    if (tab == std::string::npos) return;
    size_t end = script.find("<TAB", tab + kTab.size());
    if (end == std::string::npos) end = script.size();
    for (const char* control : {"Text(", "CheckBox("}) {
        for (size_t at = script.find(control, tab); at != std::string::npos && at < end; at = script.find(control, at + 1)) {
            // The rectangle's four numbers, kept with their separators.
            size_t pos = at + std::char_traits<char>::length(control), numberEnd = 0;
            size_t starts[4];
            int values[4];
            bool ok = true;
            for (int i = 0; i < 4 && ok; ++i) {
                while (pos < script.size() && (script[pos] == ' ' || script[pos] == ',')) ++pos;
                starts[i] = pos;
                ok = Number(script, pos, values[i], numberEnd);
                pos = numberEnd;
            }
            if (!ok || values[1] < kVideoFirstRow || (values[1] - kVideoFirstRow) % kVideoRow != 0) continue;
            const int row = (values[1] - kVideoFirstRow) / kVideoRow;
            if (row >= kVideoRowCount) continue;
            const int y0 = kSpreadFirstRow + (ownRows + row) * kSpreadRow, y1 = y0 + (values[3] - values[1]);
            // The later number first, so that the earlier one's position still holds.
            size_t y1End = 0, y0End = 0;
            int unused = 0;
            Number(script, starts[3], unused, y1End);
            script.replace(starts[3], y1End - starts[3], std::to_string(y1));
            Number(script, starts[1], unused, y0End);
            script.replace(starts[1], y0End - starts[1], std::to_string(y0));
            end = script.find("<TAB", tab + kTab.size());
            if (end == std::string::npos) end = script.size();
        }
    }
}

// After the line of the Audio tab's closing brace, the brace that matches the one opening its body; the speed rows' Define before
// the options dialog.
void AddControlsTab(std::string& script)
{
    const size_t dialog = script.find(kDialog);
    if (dialog == std::string::npos || script.find(kLastTab, dialog) == std::string::npos) return;
    const size_t dialogLine = script.rfind('\n', dialog);
    if (dialogLine == std::string::npos) return;
    InsertRows(script, dialogLine, kPadOnly, std::size(kPadOnly));
    const size_t tab = script.find(kLastTab);
    if (tab == std::string::npos) return;
    size_t at = script.find('{', tab);
    if (at == std::string::npos) return;
    for (int depth = 0; at < script.size(); ++at) {
        if (script[at] == '{') ++depth;
        if (script[at] == '}' && --depth == 0) break;
    }
    const size_t lineEnd = script.find('\n', at);
    if (lineEnd == std::string::npos) return;
    InsertRows(script, lineEnd, kControlsTab, std::size(kControlsTab));
}

} // namespace

void SetPadPointer(bool available)
{
    g_padPointer = available;
}

bool PadPointer()
{
    return g_padPointer;
}

std::string Apply(std::string script)
{
    // A touch screen has the speed sliders in its Controls tab, and room for a finger between the Video rows.
    if (g_padPointer && !g_touchScreen) AddSpeedSliders(script);
    if (g_touchScreen) {
        const std::vector<std::string> own =
                ui::WideScreenAvailable() ? OwnVideoLines({kWideScreenRow, kFpsRow}) : OwnVideoLines({kFpsRow});
        SpreadVideoRows(script, ui::WideScreenAvailable() ? 2 : 1);
        const std::vector<std::string_view> views(own.begin(), own.end());
        AddVideoRows(script, views.data(), views.size());
        AddControlsTab(script);
    }
    return script;
}

void SetTouchScreen(bool available)
{
    g_touchScreen = available;
}

bool TouchScreen()
{
    return g_touchScreen;
}

bool ConditionalDefine(const char* name)
{
    return name && kPadOnlyName == name;
}

bool DefineShown(const char* name)
{
    if (!ConditionalDefine(name)) return true;
    return !game::settings_store::Exists() || !game::settings_store::Current().m_touchControls;
}

} // namespace obl::own_controls
