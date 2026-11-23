// OpenBeach's own controls in the original dialogs' scripts (game/ui/own_controls.h).

#include "unit_test.h"

#include "game/ui/own_controls.h"

#include <string>

using namespace obl;

namespace {

// A made-up tab: only the names Apply looks for (the tab, the anchor) and the reset action are those of a real layout.
const std::string kVideoTab = "  <TABVideoOptions(1, 2, 3, 4, TAB_IMAGE, TAB_TIP)>\n"
                              "  {\n"
                              "    CheckBox(10, 20, 30, 40, BOX_IMAGE, GAME_NUDGE, BOX_TIP)\n"
                              "\n"
                              "    Button(10, 90, 30, 99, BUTTON_IMAGE, BUTTON_TEXT, ResetVideoOptions, None, BUTTON_TIP)\n"
                              "  }\n";

} // namespace

OBL_TEST_CASE("own_controls.controls_tab_follows_the_audio_tab")
{
    // The Audio tab's body holds a nested brace (a radio group): the tab ends at the brace that matches its own.
    const std::string script =
            "// options\nDialogItem InGameOptions(CY10, 190, 515, 481, P, 10, 1)\n{\n"
            "    <TABAudioOptions(439, 131, 483, 162, A, B)>\n    {\n      RadioGroup(1, 2, 3, 4, G, None)\n      {\n"
            "      }\n      Button(1, 2, 3, 4, C, D, ResetAudioOptions, None, E)\n    }\n  }\n  Button(9, 9, 9, 9, OK)\n";
    own_controls::SetTouchScreen(false);
    OBL_CHECK(own_controls::Apply(script) == script); // no touch screen, no tab
    own_controls::SetTouchScreen(true);
    const std::string out = own_controls::Apply(script);
    const size_t reset = out.find("ResetAudioOptions"), tab = out.find("<TABOblControls(439, 162, 483, 193, OBL_options_t_controls");
    const size_t ok = out.find("Button(9, 9, 9, 9, OK)");
    OBL_CHECK(reset != std::string::npos && tab != std::string::npos && ok != std::string::npos);
    OBL_CHECK(reset < tab && tab < ok); // after the Audio tab, inside the tab window
    OBL_CHECK(out.find("    }\n\n        // controls tab") != std::string::npos); // right after the Audio tab's closing line
    for (const char* name : {"TOUCH_CONTROLS", "MAGNIFIER", "BAR_AUTOHIDE", "BAR_HIDE_DELAY", "BAR_HIDE_SECONDS"})
        OBL_CHECK(out.find(name) != std::string::npos);
    own_controls::SetTouchScreen(false);
}

OBL_TEST_CASE("own_controls.touch_screens_spread_the_video_rows")
{
    const std::string video = "// options\nDialogItem InGameOptions(CY10, 190, 515, 481, P, 10, 1)\n{\n"
                              "  <TABVideoOptions(1, 2, 3, 4, A, B)>\n  {\n"
                              "    Text(32, 73, 176, 88, RES, 0, 0, 1)\n"
                              "    Text(32, 103, 176, 118, SEA, 0, 0, 1)\n"
                              "    CheckBox(194, 103, 210, 118, BOX, GAME_SEA, TIP)\n"
                              "    Text(32, 148, 176, 163, NUDGE, 0, 0, 1)\n"
                              "    CheckBox(194,148,210,163, BOX, GAME_NUDGE, TIP)\n"
                              "    Button(194, 242, 359, 266, BIG, RESET, ResetVideoOptions, None, TIP)\n  }\n"
                              "  <TABAudioOptions(1, 2, 3, 4, A, B)>\n  {\n    Text(32, 103, 176, 118, MUSIC, 0, 0, 1)\n  }\n";
    own_controls::SetPadPointer(true);
    own_controls::SetTouchScreen(true);
    const std::string out = own_controls::Apply(video);
    OBL_CHECK(out.find("Text(32, 73, 176, 88, RES") != std::string::npos); // the resolution row stays
    // One column, 23 apart from 100: the frame counter first (no Wide screen without a known screen), then the script's rows.
    OBL_CHECK(out.find("CheckBox(194, 100, 210, 115, UI3_fieldcheckbox, FPS_COUNTER") != std::string::npos);
    OBL_CHECK(out.find("CheckBox(194, 123, 210, 138, BOX, GAME_SEA") != std::string::npos);
    OBL_CHECK(out.find("Text(32, 192, 176, 207, NUDGE") != std::string::npos); // the fourth of the script's rows
    OBL_CHECK(out.find("CheckBox(194,192,210,207, BOX, GAME_NUDGE") != std::string::npos);
    OBL_CHECK(out.find("Button(194, 242, 359, 266") != std::string::npos);
    OBL_CHECK(out.find("Text(32, 103, 176, 118, MUSIC") != std::string::npos); // the Audio tab is left alone
    OBL_CHECK(out.find("FPS_COUNTER") < out.find("<TABAudioOptions"));
    OBL_CHECK(out.find("WIDE_SCREEN") == std::string::npos);
    // The speed sliders are in the Controls tab, not the Video tab.
    const size_t videoTab = out.find("<TABVideoOptions"), cursor = out.find("CURSOR_SPEED");
    OBL_CHECK(videoTab != std::string::npos && cursor != std::string::npos && cursor < videoTab); // in the Define before the dialog
    OBL_CHECK(out.find("OblPadOnly", out.find("<TABOblControls")) != std::string::npos); // which the Controls tab names
    own_controls::SetTouchScreen(false);
    own_controls::SetPadPointer(false);
}

OBL_TEST_CASE("own_controls.no_stick_leaves_the_script_alone")
{
    own_controls::SetPadPointer(false);
    OBL_CHECK(own_controls::Apply(kVideoTab) == kVideoTab);
}

OBL_TEST_CASE("own_controls.slider_follows_the_nudge_scroll_row")
{
    own_controls::SetPadPointer(true);
    const std::string script = own_controls::Apply(kVideoTab);
    const size_t nudge = script.find("GAME_NUDGE");
    const size_t slider = script.find("Slider(206, 163, 381, 178, CURSOR_SPEED");
    const size_t reset = script.find("ResetVideoOptions");
    OBL_CHECK(nudge != std::string::npos && slider != std::string::npos && reset != std::string::npos);
    OBL_CHECK(nudge < slider && slider < reset); // inside the Video tab, before its reset button
    OBL_CHECK(script.find("Text(32, 163, 176, 178, OBL_CURSOR_SPEED") != std::string::npos);
    const size_t scroll = script.find("Slider(206, 178, 381, 193, SCROLL_SPEED");
    OBL_CHECK(scroll != std::string::npos && slider < scroll && scroll < reset); // the scroll speed row follows the cursor speed row
    own_controls::SetPadPointer(false);
}

OBL_TEST_CASE("own_controls.crlf_scripts_keep_their_line_breaks")
{
    own_controls::SetPadPointer(true);
    std::string crlf;
    for (char c : kVideoTab) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    const std::string script = own_controls::Apply(crlf);
    for (size_t i = 0; i < script.size(); ++i)
        if (script[i] == '\n') OBL_CHECK(i > 0 && script[i - 1] == '\r');
    OBL_CHECK(script.find("CURSOR_SPEED") != std::string::npos);
    own_controls::SetPadPointer(false);
}

OBL_TEST_CASE("own_controls.other_layouts_are_untouched")
{
    own_controls::SetPadPointer(true);
    const std::string other = "DialogItem BuildPanel(0, 0, 10, 10, X, 1, 1)\n{\n  CheckBox(1, 1, 2, 2, G, GAME_NUDGE, T)\n}\n";
    OBL_CHECK(own_controls::Apply(other) == other); // no Video tab: nothing to attach to
    own_controls::SetPadPointer(false);
}

OBL_TEST_CASE("own_controls.anchor_in_a_later_tab_is_not_the_video_tab")
{
    own_controls::SetPadPointer(true);
    const std::string script = "  <TABVideoOptions(1, 2, 3, 4, A, B)>\n  {\n  }\n  <TABAudioOptions(1, 2, 3, 4, A, B)>\n  {\n"
                               "    CheckBox(10, 20, 30, 40, C, GAME_NUDGE, D)\n  }\n";
    OBL_CHECK(own_controls::Apply(script) == script);
    own_controls::SetPadPointer(false);
}
