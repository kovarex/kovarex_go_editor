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
#include <future>
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
  // The interface scale's controls: a step up or down, or back to automatic.
  void scale(Command command);
  void updateTitle();

  // Asks about unsaved changes, if there are any, and then does `then`.
  void guard(After then, const std::filesystem::path& file = {});
  void proceed();

  void newGame();
  bool open(const std::filesystem::path& file);
  bool save(const std::filesystem::path& file);
  void saveOrAsk();
  void showFiles(bool saving);
  // The game to AI Sensei for review: its upload page, opened in the web
  // browser with the game already on it.
  void sendToAiSensei();

  // The file's name, or "Untitled" before it has one.
  std::string name() const;
  std::filesystem::path folder() const;

  // Read before the window opens, which is sized from it.
  Settings settings = Settings::load();
  Settings applied  = this->settings;  // what the window was last set to
  int      shownScale = 0;             // the interface scale the GUI is drawn at

  // The stones painted and the wood decoded while the window opens, which
  // takes the graphics driver a good third of a second: begun before it.
  std::future<ui::GoSprites::Pixels> pictures = std::async(std::launch::async, &ui::GoSprites::prepare);

  Window       window{ this->settings };
  ui::GuiLayer gui{ this->settings, this->pictures };
  ui::Shortcuts shortcuts;

  std::unique_ptr<Game>  game;
  std::filesystem::path  path;  // empty until the game is saved or was opened

  After                 after = After::None;
  std::filesystem::path afterFile;
  bool                  saving = false;  // the file browser is up for Save as, not Open

  std::string shownTitle;
  bool        quitRequested = false;
};
