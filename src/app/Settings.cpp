#include <app/Settings.hpp>

#include <app/IniFile.hpp>

#include <raylib.h>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr int MIN_WINDOW_W = 320;
constexpr int MIN_WINDOW_H = 240;

// Where the window was before it went windowed fullscreen.
struct Restore {
  bool    active    = false;
  bool    maximized = false;
  Vector2 position  = { 0.0f, 0.0f };
  int     width     = 0;
  int     height    = 0;
} restore;

}  // namespace

Settings Settings::load()
{
  Settings s;
  IniFile ini;
  if (!ini.load(path())) return s;

  s.graphics.windowedFullscreen = ini.getBool("graphics", "windowed-fullscreen", s.graphics.windowedFullscreen);
  s.graphics.vsync              = ini.getBool("graphics", "vsync", s.graphics.vsync);
  s.graphics.fpsLimit           = std::max(0, ini.getInt("graphics", "fps-limit", s.graphics.fpsLimit));
  s.graphics.automaticScale     = ini.getBool("graphics", "automatic-interface-scale", s.graphics.automaticScale);
  s.graphics.interfaceScale     = ClampedInterfaceScale(ini.getInt("graphics", "interface-scale", s.graphics.interfaceScale));
  s.graphics.tooltipDelay       = ini.getInt("graphics", "tooltip-delay", s.graphics.tooltipDelay);
  if (s.graphics.tooltipDelay != Settings::Graphics::TOOLTIPS_NEVER) {
    s.graphics.tooltipDelay = std::clamp(s.graphics.tooltipDelay, 0, Settings::Graphics::MAX_TOOLTIP_DELAY);
  }

  s.window.width     = std::max(MIN_WINDOW_W, ini.getInt("window", "width", s.window.width));
  s.window.height    = std::max(MIN_WINDOW_H, ini.getInt("window", "height", s.window.height));
  s.window.maximized = ini.getBool("window", "maximized", s.window.maximized);

  s.board.coordinates = ini.getBool("board", "coordinates", s.board.coordinates);
  s.board.moveNumbers = ini.getBool("board", "move-numbers", s.board.moveNumbers);
  s.board.nextMoves   = ini.getBool("board", "next-moves", s.board.nextMoves);
  s.board.treeNumbers = ini.getBool("board", "tree-move-numbers", s.board.treeNumbers);

  s.newGame.width    = std::clamp(ini.getInt("new-game", "width", s.newGame.width), 2, MAX_BOARD);
  s.newGame.height   = std::clamp(ini.getInt("new-game", "height", s.newGame.height), 2, MAX_BOARD);
  s.newGame.handicap = std::clamp(ini.getInt("new-game", "handicap", s.newGame.handicap), 0, 9);
  s.newGame.komi     = ini.getString("new-game", "komi", s.newGame.komi);
  s.newGame.black    = ini.getString("new-game", "black", s.newGame.black);
  s.newGame.white    = ini.getString("new-game", "white", s.newGame.white);

  s.folder = ini.getString("files", "folder", s.folder);

  // A control missing from the file keeps its default; one written with
  // nothing after the = was cleared on purpose.
  const std::vector<ui::Control>& controls = ui::AllControls();
  for (size_t i = 0; i < controls.size(); ++i) {
    ui::Bindings::Pair& keys = s.controls.keys[i];
    const std::string   id   = controls[i].id;
    keys.primary     = ui::ParseKey(ini.getString("controls", id, ui::KeyText(keys.primary)));
    keys.alternative = ui::ParseKey(ini.getString("controls", id + "-alternative", ui::KeyText(keys.alternative)));
  }
  return s;
}

void Settings::save() const
{
  IniFile ini;
  ini.setBool("graphics", "windowed-fullscreen", this->graphics.windowedFullscreen);
  ini.setBool("graphics", "vsync", this->graphics.vsync);
  ini.setInt("graphics", "fps-limit", this->graphics.fpsLimit);
  ini.setBool("graphics", "automatic-interface-scale", this->graphics.automaticScale);
  ini.setInt("graphics", "interface-scale", this->graphics.interfaceScale);
  ini.setInt("graphics", "tooltip-delay", this->graphics.tooltipDelay);

  ini.setInt("window", "width", this->window.width);
  ini.setInt("window", "height", this->window.height);
  ini.setBool("window", "maximized", this->window.maximized);

  ini.setBool("board", "coordinates", this->board.coordinates);
  ini.setBool("board", "move-numbers", this->board.moveNumbers);
  ini.setBool("board", "next-moves", this->board.nextMoves);
  ini.setBool("board", "tree-move-numbers", this->board.treeNumbers);

  ini.setInt("new-game", "width", this->newGame.width);
  ini.setInt("new-game", "height", this->newGame.height);
  ini.setInt("new-game", "handicap", this->newGame.handicap);
  ini.set("new-game", "komi", this->newGame.komi);
  ini.set("new-game", "black", this->newGame.black);
  ini.set("new-game", "white", this->newGame.white);

  ini.set("files", "folder", this->folder);

  const std::vector<ui::Control>& all = ui::AllControls();
  for (size_t i = 0; i < all.size(); ++i) {
    const ui::Bindings::Pair& keys = this->controls.keys[i];
    const std::string         id   = all[i].id;
    ini.set("controls", id, keys.primary.isSet() ? ui::KeyText(keys.primary) : "");
    ini.set("controls", id + "-alternative", keys.alternative.isSet() ? ui::KeyText(keys.alternative) : "");
  }

  if (!ini.save(path(), "Go editor settings. The editor rewrites this file, so edit it while the editor isn't running.")) {
    TraceLog(LOG_WARNING, "SETTINGS: Couldn't write %s", path().string().c_str());
  }
}

std::filesystem::path Settings::path()
{
  if (const char* appData = std::getenv("APPDATA")) {
    return std::filesystem::path(appData) / "GoEditor" / "config.ini";
  }
  return "config.ini";
}

void ApplySettings(const Settings& settings)
{
  if (IsWindowState(FLAG_VSYNC_HINT) != settings.graphics.vsync) {
    if (settings.graphics.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else                         ClearWindowState(FLAG_VSYNC_HINT);
  }
  SetWindowedFullscreen(settings.graphics.windowedFullscreen);
  SetTargetFPS(settings.graphics.fpsLimit);
}

void SetWindowedFullscreen(bool on)
{
  if (on == restore.active) return;

  if (on) {
    restore.maximized = IsWindowMaximized();
    if (restore.maximized) RestoreWindow();  // so the size saved is the one to go back to
    restore.position = GetWindowPosition();
    restore.width    = GetScreenWidth();
    restore.height   = GetScreenHeight();

    const int     monitor = GetCurrentMonitor();
    const Vector2 origin  = GetMonitorPosition(monitor);
    SetWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowPosition(int(origin.x), int(origin.y));
    // One pixel taller than the monitor. A GL window that covers it exactly
    // gets promoted by the driver to what is effectively exclusive fullscreen
    // -- a black flash on alt-tab, nothing able to draw over it -- which is
    // what this mode is here to avoid. The spare row is off the bottom edge.
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor) + 1);
  } else {
    ClearWindowState(FLAG_WINDOW_UNDECORATED);
    SetWindowSize(restore.width, restore.height);
    SetWindowPosition(int(restore.position.x), int(restore.position.y));
    if (restore.maximized) MaximizeWindow();
  }
  restore.active = on;
}

bool IsWindowedFullscreen()
{
  return restore.active;
}
