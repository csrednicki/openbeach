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
