// Tuning. Everything worth fiddling with that isn't a style lives here.
//
// The board's *look* is not here: the points, stones and lines are Agui
// widgets, so their colours and backgrounds are styles, in ui/Theme.cpp with
// the rest of the theme, and the stones themselves are painted in
// ui/GoSprites.cpp.

#pragma once

#include <raylib.h>

namespace cfg {

// What the window and the task bar call it.
constexpr const char* APP_NAME = "Go Editor";

// What the window is cleared to, behind the widgets.
constexpr Color BACKGROUND_COLOR = Color{ 24, 24, 28, 255 };

// Smaller than this and the side panel and the board don't both fit.
constexpr int MIN_WINDOW_W = 800;
constexpr int MIN_WINDOW_H = 560;

}  // namespace cfg
