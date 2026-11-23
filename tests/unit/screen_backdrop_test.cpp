// The full-screen picture the game drew last (game/ui/screen_backdrop.h).

#include "unit_test.h"

#include "game/ui/screen_backdrop.h"

namespace backdrop = obl::ui::screen_backdrop;

OBL_TEST_CASE("screen_backdrop.key_is_the_lowercase_file_name")
{
    OBL_CHECK(backdrop::KeyOf("data\\screens\\MainMenu.tga") == "mainmenu");
    OBL_CHECK(backdrop::KeyOf("data/screens/loading_1.tga") == "loading_1");
    OBL_CHECK(backdrop::KeyOf("FrontScreen.tga") == "frontscreen");
    backdrop::Shown("data/screens/loading_2.tga");
    OBL_CHECK(backdrop::Current() == "loading_2");
}

OBL_TEST_CASE("screen_backdrop.the_scenario_picture_moves_only_on_its_screen")
{
    backdrop::SetPictureShift("scenarioscreen", 13, -2);
    int x = 0, y = 0;
    backdrop::Shown("ScenarioScreen.tga");
    backdrop::PictureShift(x, y);
    OBL_CHECK(x == 13 && y == -2);
    backdrop::Shown("data/screens/mainmenu.tga");
    backdrop::PictureShift(x, y);
    OBL_CHECK(x == 0 && y == 0);
    backdrop::SetPictureShift("", 0, 0);
}
