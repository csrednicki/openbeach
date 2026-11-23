#pragma once

#include <string>

// Controls OpenBeach adds to the original game's dialogs. The converted layouts are the original scripts, byte for byte (the
// converter never edits them), so an own control is inserted into the script text after it is read, and its texts come from
// own_texts.h. Today: the pointer speed sliders in the Video options, for builds whose pointer is a thumb stick, and the Controls
// tab of touch screens (its icon: own_sprites.h).
namespace obl::own_controls {

// Whether this host moves the pointer with a stick (set once by the platform's input backend); the speed slider is then added.
void SetPadPointer(bool available);
bool PadPointer();

// Whether this host is a touch screen (Android); the options then get the Controls tab after the Audio tab.
void SetTouchScreen(bool available);
bool TouchScreen();

// A Define of the own controls that shows only while its condition holds (GuiDefine): the Controls tab's speed rows, while the
// touch controls are off (the D-pad and touchpad). Any other Define is always shown.
bool ConditionalDefine(const char* name);
bool DefineShown(const char* name);

// The text of a layout file as the dialogs should parse it: `script` with the own controls inserted. Without the options' tabs, or
// without a stick and a touch screen, it is `script` unchanged.
std::string Apply(std::string script);

} // namespace obl::own_controls
