// The pages that come up over the editor -- New game, Game info, Settings,
// Controls, the file browser, and the unsaved-changes question -- one at a time,
// with a dark sheet over the editor behind. Each is a window of its own,
// dragged by its title, centred when it opens.
//
// A page says what the player chose through takeAction(); doing it is the
// App's business.

#pragma once

#include <ui/FilePage.hpp>
#include <ui/GameInfoPage.hpp>
#include <ui/AboutPage.hpp>
#include <ui/ConfirmPage.hpp>
#include <ui/ControlsPage.hpp>
#include <ui/NewGamePage.hpp>
#include <ui/SettingsPage.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/EmptyWidget.hpp>

#include <filesystem>
#include <optional>
#include <string>

struct Settings;

namespace agui {
class Gui;
}

namespace ui {

class Theme;

class Pages : public agui::GenericTargetable {
public:
  enum class Page { None, NewGame, GameInfo, Settings, Controls, About, Files, Confirm };

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
    SaveControls,     // Controls' Confirm: keep the keys the page changed
    OpenProjectPage,  // About's link: the project's page in the browser
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

  // The search of the page that is up, if it has one: focusSearch() opens
  // it; cancelSearch() closes it, and says whether there was one open for
  // Esc to close rather than the page. The shortcut is for their tooltips.
  void focusSearch();
  bool cancelSearch();
  void setSearchShortcut(const std::string& keys);

  Action takeAction();
  const std::filesystem::path& chosenFile() const { return this->chosen; }

  NewGamePage  newGame;
  GameInfoPage gameInfo;
  SettingsPage settings;
  ControlsPage controls;
  AboutPage    about;
  FilePage     files;
  ConfirmPage  confirm;

private:
  agui::Window* window(Page page);
  SearchBar*    search();  // the page's, if it has one
  void          finish(Action action);

  agui::Gui& gui;

  // Goes into the Gui before the pages, so it darkens the editor behind
  // without touching the page over it.
  agui::EmptyWidget dimmer;

  Action                pending = Action::None;
  Page                  page    = Page::None;
  std::filesystem::path chosen;
  std::optional<std::string> searchShortcut;  // what the search tooltips were given
  bool recentre = true;
  int  lastScreenWidth = 0, lastScreenHeight = 0;
};

}  // namespace ui
