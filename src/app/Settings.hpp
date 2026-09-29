// The user's settings, and the config.ini they're kept in between runs.
// Plain data: the settings page edits it in place, and App applies and saves
// whatever changed.

#pragma once

#include <game/Game.hpp>
#include <ui/Controls.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

// Someone met in a room on a relay, remembered so as to meet again: the two
// editors keep a room key of their own there (see net::Type::Pair), and
// whichever of them comes to it first opens it.
struct Contact {
  std::string identity;  // theirs: how they are known when met again
  std::string name;      // as they last called themselves
  std::string relay;     // the relay's address
  std::string key;       // the pair's room on it
};

// What the Online page was last set to: the name to go by in a shared game,
// the port to host on, and where to join -- an address, and a room code when
// that is a relay. And the people met there before.
struct OnlineSetup {
  std::string name;
  int         port = 27272;
  std::string address;
  std::string room;
  // This editor's identity: made up the first time, then kept.
  std::string          identity;
  std::vector<Contact> contacts;
};

struct Settings {
  struct Graphics {
    bool windowedFullscreen = false;  // a borderless window over the whole monitor
    bool vsync              = true;
    int  fpsLimit           = 0;  // 0 for none; vsync still holds it to the refresh rate

    // How big the whole GUI is drawn -- board, panel and all. Automatic, as
    // Factorio does it, follows the window's size (AutomaticInterfaceScale),
    // so maximizing the window makes everything bigger. Otherwise it is
    // interfaceScale, in percent, from MIN_SCALE to MAX_SCALE in SCALE_STEP
    // steps. Ctrl and the numpad's + and - step it from anywhere (from the
    // automatic value, which they leave), and Ctrl and numpad 0 go back to
    // automatic.
    bool automaticScale = true;
    int  interfaceScale = 100;

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
    // To start with, a full HD screen less the window's frame: what the
    // automatic interface scale is measured against (see
    // AutomaticInterfaceScale), so it starts at 125%.
    int  width     = 1920 - 64;
    int  height    = 1080 - 64;
    bool maximized = false;
  };

  Graphics graphics;
  Board    board;
  Window   window;

  // The keys for each control; see ui::AllControls.
  ui::Bindings controls;

  // What the New game page is set up for: the last game started.
  GameSetup newGame;

  OnlineSetup online;

  // Where the file browser was last, as UTF-8.
  std::string folder;

  // A missing file or key keeps the default; out-of-range values are clamped.
  static Settings load();
  void            save() const;

  // %APPDATA%\GoEditor\config.ini on Windows, ~/.config/GoEditor/config.ini
  // (or under $XDG_CONFIG_HOME) on Linux; config.ini in the working directory
  // where there is neither.
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

// The interface scale for a window `width` x `height` pixels. Worked out as
// Factorio's automatic UI scale is -- in proportion to a 1920x1080 screen
// less the window's frame, by whichever way it is tighter, rounded down to a
// step -- and then one step bigger: a Go editor shows much less than Factorio
// does, and has room to show it larger. So 125% on full HD. Within the range.
constexpr int AutomaticInterfaceScale(int width, int height)
{
  using G = Settings::Graphics;
  constexpr int FULL_HD_W = 1920 - 64, FULL_HD_H = 1080 - 64;
  const int fit     = std::min(width * 100 / FULL_HD_W, height * 100 / FULL_HD_H);
  const int stepped = fit / G::SCALE_STEP * G::SCALE_STEP + G::SCALE_STEP;
  return std::clamp(stepped, G::MIN_SCALE, G::MAX_SCALE);
}

// The scale the GUI is drawn at, for a window this size.
constexpr int EffectiveInterfaceScale(const Settings::Graphics& graphics, int width, int height)
{
  return graphics.automaticScale ? AutomaticInterfaceScale(width, height) : graphics.interfaceScale;
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
