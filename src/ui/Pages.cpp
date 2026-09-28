#include <ui/Pages.hpp>

#include <app/Settings.hpp>
#include <ui/SearchBar.hpp>
#include <ui/Theme.hpp>

#include <Agui/Gui.hpp>

namespace ui {

Pages::Pages(agui::Gui& gui, Theme& theme, Settings& config)
    : newGame(theme, config.newGame, [this] { this->finish(Action::StartGame); }, [this] { this->finish(Action::Back); })
    , gameInfo(theme, [this] { this->finish(Action::ApplyGameInfo); }, [this] { this->finish(Action::Back); })
    , settings(theme, config, [this] { this->pending = Action::Associate; }, [this] { this->finish(Action::SaveSettings); },
               [this] { this->finish(Action::DiscardSettings); })
    , controls(theme, config, [this] { this->finish(Action::SaveControls); }, [this] { this->finish(Action::Back); })
    , about(theme, [this] { this->pending = Action::OpenProjectPage; }, [this] { this->finish(Action::Back); })
    , files(theme,
            [this](const std::filesystem::path& path) {
              this->chosen = path;
              this->finish(Action::FileChosen);
            },
            [this] { this->finish(Action::Back); })
    , confirm(theme, [this] { this->finish(Action::Save); }, [this] { this->finish(Action::Discard); }, [this] { this->finish(Action::Back); })
    , gui(gui)
{
  this->dimmer.style.setParent(&theme.dimmer);
  gui.add(&this->dimmer);  // before the pages, so they are not dimmed too
  for (Page p : { Page::NewGame, Page::GameInfo, Page::Settings, Page::Controls, Page::About, Page::Files, Page::Confirm }) {
    gui.add(this->window(p));
  }
  this->open(Page::None);
}

Pages::~Pages()
{
  for (Page p : { Page::Confirm, Page::Files, Page::About, Page::Controls, Page::Settings, Page::GameInfo, Page::NewGame }) {
    this->gui.remove(this->window(p));
  }
  this->gui.remove(&this->dimmer);
}

agui::Window* Pages::window(Page p)
{
  switch (p) {
  case Page::NewGame:  return &this->newGame.root();
  case Page::GameInfo: return &this->gameInfo.root();
  case Page::Settings: return &this->settings.root();
  case Page::Controls: return &this->controls.root();
  case Page::About:    return &this->about.root();
  case Page::Files:    return &this->files.root();
  case Page::Confirm:  return &this->confirm.root();
  case Page::None:     break;
  }
  return nullptr;
}

void Pages::open(Page p)
{
  this->page     = p;
  this->recentre = true;
  this->dimmer.setVisible(p != Page::None);
  for (Page each : { Page::NewGame, Page::GameInfo, Page::Settings, Page::Controls, Page::About, Page::Files, Page::Confirm }) {
    this->window(each)->setVisible(each == p);
  }
  // Whatever had the keyboard is behind the sheet now.
  this->gui.clearFocus();
}

void Pages::finish(Action action)
{
  this->pending = action;
  this->close();
}

void Pages::layout(int screenWidth, int screenHeight)
{
  if (screenWidth != this->lastScreenWidth || screenHeight != this->lastScreenHeight) {
    this->lastScreenWidth  = screenWidth;
    this->lastScreenHeight = screenHeight;
    this->recentre = true;
  }

  this->dimmer.setLocation(0, 0);
  this->dimmer.setSize(screenWidth, screenHeight, agui::SetSizeInfo());

  this->controls.fit(screenHeight);

  agui::Window* shown = this->window(this->page);
  if (!shown) return;
  if (this->recentre && shown->getWidth() > 0) {  // not before its first layout
    shown->setLocation((screenWidth - shown->getWidth()) / 2, (screenHeight - shown->getHeight()) / 2);
    this->recentre = false;
  }
  shown->ensureWholeWindowIsOnScreen();
}

SearchBar* Pages::search()
{
  switch (this->page) {
  case Page::Settings: return &this->settings.searchBar();
  case Page::Controls: return &this->controls.searchBar();
  default:             return nullptr;
  }
}

void Pages::focusSearch()
{
  if (SearchBar* bar = this->search()) bar->focusSearch();
}

bool Pages::cancelSearch()
{
  SearchBar* bar = this->search();
  return bar && bar->clearAndHide();
}

void Pages::setSearchShortcut(const std::string& keys)
{
  if (this->searchShortcut == keys) return;
  this->searchShortcut = keys;
  this->settings.searchBar().setShortcut(keys);
  this->controls.searchBar().setShortcut(keys);
}

Pages::Action Pages::takeAction()
{
  const Action a = this->pending;
  this->pending = Action::None;
  return a;
}

}  // namespace ui
