// The application: the window, the game being edited and the file it came
// from, and the frame loop. Each frame the shortcuts are read first, then the
// Gui runs, then whatever either asked for is done.
//
// The App is what deals with files: opening, saving, starting a new game, and
// asking first when any of those -- or closing the window -- would throw
// away unsaved changes.

#pragma once

#include <app/Settings.hpp>
#include <game/Game.hpp>
#include <ui/Commands.hpp>
#include <ui/GuiLayer.hpp>
#include <ui/Shortcuts.hpp>

#include <filesystem>
#include <memory>
#include <string>

class App {
public:
  App();

  // Runs until the window closes.
  int run();

private:
  // Opens the window on construction and closes it on destruction. Declared
  // before every member that needs the GL context, so it brackets them.
  struct Window {
    explicit Window(const Settings& settings);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
  };

  // What to do once the unsaved-changes question has been answered.
  enum class After { None, NewGame, Open, Quit };

  void frame();
  void handle(Command command);
  void handlePages();
  void handleDroppedFiles();
  void handleClose();
  void updateSettings();
  // Settings' Back, or Esc on it: everything the page changed goes back to
  // how it was when the page opened. The App applies it on the next frame.
  void discardSettings();
  void updateTitle();

  // Asks about unsaved changes, if there are any, and then does `then`.
  void guard(After then, const std::filesystem::path& file = {});
  void proceed();

  void newGame();
  bool open(const std::filesystem::path& file);
  bool save(const std::filesystem::path& file);
  void saveOrAsk();
  void showFiles(bool saving);

  // The file's name, or "Untitled" before it has one.
  std::string name() const;
  std::filesystem::path folder() const;

  // Read before the window opens, which is sized from it.
  Settings settings = Settings::load();
  Settings applied  = this->settings;  // what the window was last set to
  Settings kept     = this->settings;  // as the settings page found them, to go back to

  Window       window{ this->settings };
  ui::GuiLayer gui{ this->settings };
  ui::Shortcuts shortcuts;

  std::unique_ptr<Game>  game;
  std::filesystem::path  path;  // empty until the game is saved or was opened

  After                 after = After::None;
  std::filesystem::path afterFile;
  bool                  saving = false;  // the file browser is up for Save as, not Open

  std::string shownTitle;
  bool        quitRequested = false;
};
