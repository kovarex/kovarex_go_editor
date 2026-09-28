// The user's settings, and the config.ini they're kept in between runs.
// Plain data: the settings page edits it in place, and App applies and saves
// whatever changed.

#pragma once

#include <game/Game.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

struct Settings {
  struct Graphics {
    bool windowedFullscreen = false;  // a borderless window over the whole monitor
    bool vsync              = true;
    int  fpsLimit           = 0;  // 0 for none; vsync still holds it to the refresh rate

    // How big the whole GUI is drawn -- board, panel and all -- in percent,
    // from MIN_SCALE to MAX_SCALE in SCALE_STEP steps. Ctrl and the numpad's +
    // and - step it from anywhere, as well as the settings page.
    int interfaceScale = 100;

    // The GUI atlas is drawn at half its pixel size, so it stays sharp up to
    // 200%. Below 75% the text stops being readable.
    static constexpr int MIN_SCALE  = 75;
    static constexpr int MAX_SCALE  = 200;
    static constexpr int SCALE_STEP = 25;

    // How long the mouse rests on something before its tooltip shows, in
    // milliseconds, or TOOLTIPS_NEVER. Holding Shift shows them at once
    // whatever this says.
    int tooltipDelay = 200;

    static constexpr int MAX_TOOLTIP_DELAY  = 200;
    static constexpr int TOOLTIP_DELAY_STEP = 10;
    static constexpr int TOOLTIPS_NEVER     = -1;

    bool operator==(const Graphics&) const = default;
  };

  // How the board is shown.
  struct Board {
    bool coordinates = true;
    bool moveNumbers = false;
    bool nextMoves   = true;  // faint stones where the variations from here go
    bool treeNumbers = true;  // move numbers on the game tree's stones

    bool operator==(const Board&) const = default;
  };

  // Not chosen on the settings page but remembered: how the window was left.
  struct Window {
    int  width     = 1280;
    int  height    = 800;
    bool maximized = false;
  };

  Graphics graphics;
  Board    board;
  Window   window;

  // What the New game page is set up for: the last game started.
  GameSetup newGame;

  // Where the file browser was last, as UTF-8.
  std::string folder;

  // A missing file or key keeps the default; out-of-range values are clamped.
  static Settings load();
  void            save() const;

  // %APPDATA%\GoEditor\config.ini, or config.ini in the working directory
  // where there is no %APPDATA%.
  static std::filesystem::path path();
};

// `percent` as an interface scale the editor offers: on the nearest step, and
// within the range.
constexpr int ClampedInterfaceScale(int percent)
{
  using G = Settings::Graphics;
  const int stepped = (percent + G::SCALE_STEP / 2) / G::SCALE_STEP * G::SCALE_STEP;
  return std::clamp(stepped, G::MIN_SCALE, G::MAX_SCALE);
}

// Brings the window in line with `settings`. Only touches what differs, so
// it's fine to call whenever anything might have changed.
void ApplySettings(const Settings& settings);

// Windowed fullscreen: the window loses its border and covers the monitor it's
// on, but stays an ordinary window -- not topmost, and not exclusive, so the
// monitor never changes mode and alt-tab and other windows behave. (raylib's
// ToggleBorderlessWindowed() makes it topmost, hence doing it by hand.) Going
// back puts the window where it was, maximized if it was.
void SetWindowedFullscreen(bool on);
bool IsWindowedFullscreen();
