// The game's typeface, shared by the HUD and the Agui widgets.
//
// Consolas (VS Code's default editor font on Windows) is loaded from the
// system font folder and rasterised once per pixel size, so text is crisp at
// every size instead of scaled up from one. Consolas Bold is used for bold
// text if it's there, regular Consolas if not. If Consolas itself isn't there
// (another OS, a stripped-down Windows), everything falls back to raylib's
// built-in font.

#pragma once

#include <raylib.h>

namespace ui {

// The font rasterised at `px` pixels tall. Loaded on first use; needs the window.
const ::Font& FontAt(int px, bool bold = false);

// Extra px between characters at `px`: none for the TTF, raylib's usual
// size/10 for the built-in bitmap font.
float FontSpacing(int px);

// Drop-in replacements for raylib's DrawText / MeasureText.
void Text(const char* text, int x, int y, int px, ::Color color);
int  TextWidth(const char* text, int px);

// Frees every size loaded so far. Call before CloseWindow().
void UnloadFonts();

}  // namespace ui
