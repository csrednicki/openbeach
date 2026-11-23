#include "game/game/own_texts.h"
#include "game/tables/text_load.h"

#include <array>
#include <iterator>
#include <string>

namespace obl::own_texts {

namespace {

// The languages of text_load::LanguageCode, in the order of Row::text.
constexpr std::string_view kLanguages[] = {"en", "fr", "it", "de", "es", "pl"};
constexpr size_t kLanguageCount = std::size(kLanguages);

struct Row {
    const char* key;
    std::array<const char16_t*, kLanguageCount> text;
};

constexpr Row kRows[] = {
        {"OBL_CURSOR_SPEED",
                {u"Cursor speed", u"Vitesse du curseur", u"Velocit\x00e0 del cursore", u"Cursorgeschwindigkeit", u"Velocidad del cursor",
                        u"Szybko\x015b\x0107 kursora"}},
        {"OBL_TIPTEXT_CURSOR_SPEED",
                {u"How fast the stick moves the cursor", u"La vitesse du curseur avec le joystick",
                        u"Quanto veloce il joystick muove il cursore", u"Wie schnell der Stick den Cursor bewegt",
                        u"Qu\x00e9 tan r\x00e1pido mueve el cursor el joystick",
                        u"Jak szybko dr\x0105\x017c"
                        u"ek porusza kursorem"}},
        {"OBL_SCROLL_SPEED",
                {u"Scroll speed",
                        u"Vitesse de d\x00e9"
                        u"filement",
                        u"Velocit\x00e0 di scorrimento", u"Scrollgeschwindigkeit", u"Velocidad de desplazamiento",
                        u"Szybko\x015b\x0107 przewijania"}},
        {"OBL_TIPTEXT_SCROLL_SPEED",
                {u"How fast the map scrolls",
                        u"La vitesse de d\x00e9"
                        u"filement de la carte",
                        u"Quanto veloce scorre la mappa", u"Wie schnell die Karte scrollt", u"Qu\x00e9 tan r\x00e1pido se desplaza el mapa",
                        u"Jak szybko przewija si\x0119 plansza"}},
        // The Controls tab of touch screens (ui/own_controls.h).
        {"OBL_TIPTEXT_CONTROLSTAB",
                {u"Controls options", u"Options des commandes", u"Opzioni dei controlli", u"Steuerungsoptionen", u"Opciones de control",
                        u"Opcje sterowania"}},
        {"OBL_CONTROLS_TITLE", {u"Controls", u"Commandes", u"Controlli", u"Steuerung", u"Controles", u"Sterowanie"}},
        {"OBL_TOUCH_CONTROLS",
                {u"Touch controls", u"Commandes tactiles", u"Controlli touch", u"Touch-Steuerung",
                        u"Control t\x00e1"
                        u"ctil",
                        u"Sterowanie dotykowe"}},
        {"OBL_TIPTEXT_TOUCH_CONTROLS",
                {u"On: touch gestures. Off: a virtual D-pad and a touchpad",
                        u"Activ\x00e9 : gestes tactiles. D\x00e9sactiv\x00e9 : croix directionnelle et pav\x00e9 tactile virtuels",
                        u"S\x00ec: gesti touch. No: croce direzionale e touchpad virtuali",
                        u"An: Touch-Gesten. Aus: virtuelles Steuerkreuz und Touchpad",
                        u"S\x00ed: gestos t\x00e1"
                        u"ctiles. No: cruceta y panel t\x00e1"
                        u"ctil virtuales",
                        u"W\x0142\x0105"
                        u"czone: gesty dotykowe. Wy\x0142\x0105"
                        u"czone: wirtualny pad i touchpad"}},
        {"OBL_MAGNIFIER", {u"Magnifier", u"Loupe", u"Lente", u"Lupe", u"Lupa", u"Lupa"}},
        {"OBL_TIPTEXT_MAGNIFIER",
                {u"A button above the D-pad zooms the screen 2x",
                        u"Un bouton au-dessus de la croix agrandit l'\x00e9"
                        u"cran 2x",
                        u"Un pulsante sopra la croce ingrandisce lo schermo 2x",
                        u"Ein Knopf \x00fc"
                        u"ber dem Steuerkreuz vergr\x00f6\x00df"
                        u"ert das Bild 2x",
                        u"Un bot\x00f3n sobre la cruceta ampl\x00ed"
                        u"a la pantalla 2x",
                        u"Przycisk nad padem powi\x0119ksza ekran 2x"}},
        {"OBL_BAR_AUTOHIDE",
                {u"Hide the panel", u"Masquer le panneau", u"Nascondi pannello", u"Leiste ausblenden", u"Ocultar panel", u"Ukrywaj panel"}},
        {"OBL_TIPTEXT_BAR_AUTOHIDE",
                {u"The bottom panel slides away when nothing happens", u"Le panneau du bas se cache sans activit\x00e9",
                        u"Il pannello in basso si nasconde se inattivo", u"Die untere Leiste verschwindet bei Inaktivit\x00e4t",
                        u"El panel inferior se oculta si no hay actividad", u"Dolny panel chowa si\x0119, gdy nic si\x0119 nie dzieje"}},
        {"OBL_BAR_HIDE_DELAY", {u"Hide after", u"Masquer apr\x00e8s", u"Nascondi dopo", u"Ausblenden nach", u"Ocultar tras", u"Ukryj po"}},
        {"OBL_TIPTEXT_BAR_HIDE_DELAY",
                {u"How long the panel stays after the last action",
                        u"Dur\x00e9"
                        u"e d'affichage du panneau apr\x00e8s la derni\x00e8re action",
                        u"Quanto resta il pannello dopo l'ultima azione", u"Wie lange die Leiste nach der letzten Aktion bleibt",
                        u"Cu\x00e1nto sigue el panel tras la \x00faltima acci\x00f3n", u"Jak d\x0142ugo panel zostaje po ostatniej akcji"}},
};

size_t LanguageIndex(std::string_view language)
{
    for (size_t i = 0; i < kLanguageCount; ++i)
        if (kLanguages[i] == language) return i;
    return 0;
}

} // namespace

std::vector<text::Entry> Texts(std::string_view language)
{
    const size_t lang = LanguageIndex(language);
    std::vector<text::Entry> texts;
    for (const Row& row : kRows) texts.push_back({row.key, text_load::EngineValue(row.text[lang], language == "pl")});
    return texts;
}

} // namespace obl::own_texts
