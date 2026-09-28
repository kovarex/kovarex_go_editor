// The pages that come up over the editor -- New game, Game info, Settings,
// Help, the file browser, and the unsaved-changes question -- one at a time,
// with a dark sheet over the editor behind. Each is a window of its own,
// dragged by its title, centred when it opens.
//
// A page says what the player chose through takeAction(); doing it is the
// App's business.

#pragma once

#include <ui/FilePage.hpp>
#include <ui/GameInfoPage.hpp>
#include <ui/HelpPage.hpp>
#include <ui/NewGamePage.hpp>
#include <ui/SettingsPage.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/EmptyWidget.hpp>

#include <filesystem>

struct Settings;

namespace agui {
class Gui;
}

namespace ui {

class Theme;

class Pages : public agui::GenericTargetable {
public:
  enum class Page { None, NewGame, GameInfo, Settings, Help, Files, Confirm };

  enum class Action {
    None,
    StartGame,      // New game's Start: a game of settings.newGame
    ApplyGameInfo,  // Game info's Apply: gameInfo.values()
    FileChosen,     // the file browser's Open or Save: chosenFile()
    Save,           // the unsaved-changes question, answered
    Discard,
    Associate,        // Settings: make .sgf files open here
    SaveSettings,     // Settings' Save changes: keep what the page changed
    DiscardSettings,  // Settings' Back: put back what there was when it opened
    Back,           // a page's Back or Cancel: nothing to do, and nothing waiting on it either
  };

  // Adds itself to `gui`. Do this after the editor, so the pages draw on top.
  Pages(agui::Gui& gui, Theme& theme, Settings& config);
  ~Pages();
  Pages(const Pages&) = delete;
  Pages& operator=(const Pages&) = delete;

  void open(Page page);
  void close() { this->open(Page::None); }
  bool isOpen() const { return this->page != Page::None; }
  Page current() const { return this->page; }

  // Centres a page when it opens or the screen changes size (in between, the
  // player can drag it by its title bar) and keeps it on screen. Call after
  // Gui::logic() so sizes are current.
  void layout(int screenWidth, int screenHeight);

  Action takeAction();
  const std::filesystem::path& chosenFile() const { return this->chosen; }

  NewGamePage  newGame;
  GameInfoPage gameInfo;
  SettingsPage settings;
  HelpPage     help;
  FilePage     files;
  ConfirmPage  confirm;

private:
  agui::Window* window(Page page);
  void          finish(Action action);

  agui::Gui& gui;

  // Goes into the Gui before the pages, so it darkens the editor behind
  // without touching the page over it.
  agui::EmptyWidget dimmer;

  Action                pending = Action::None;
  Page                  page    = Page::None;
  std::filesystem::path chosen;
  bool recentre = true;
  int  lastScreenWidth = 0, lastScreenHeight = 0;
};

}  // namespace ui
