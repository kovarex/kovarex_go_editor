#include <app/App.hpp>

#include <app/Platform.hpp>
#include <game/Config.hpp>
#include <game/Sgf.hpp>
#include <ui/EditorView.hpp>
#include <ui/Fonts.hpp>
#include <ui/Pages.hpp>

#include <raylib.h>
// raylib has no way to take back a click on the window's close button, so
// that one call goes to GLFW, which raylib is built on, directly.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

namespace {

// Today, as SGF writes dates.
std::string Today()
{
  const std::chrono::year_month_day today{ std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
  char text[16];
  std::snprintf(text, sizeof(text), "%04d-%02u-%02u", int(today.year()), unsigned(today.month()), unsigned(today.day()));
  return text;
}

}  // namespace

App::Window::Window(const Settings& settings)
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | (settings.graphics.vsync ? FLAG_VSYNC_HINT : 0));
  InitWindow(settings.window.width, settings.window.height, cfg::APP_NAME);
  SetWindowMinSize(cfg::MIN_WINDOW_W, cfg::MIN_WINDOW_H);
  // Esc belongs to the editor; closing is the close button's job.
  SetExitKey(KEY_NULL);

  // A size saved on a bigger monitor would hang off this one.
  const int monitor  = GetCurrentMonitor();
  const int monitorW = GetMonitorWidth(monitor);
  const int monitorH = GetMonitorHeight(monitor);
  if (GetScreenWidth() > monitorW || GetScreenHeight() > monitorH) {
    const int w = std::min(GetScreenWidth(), monitorW * 9 / 10);
    const int h = std::min(GetScreenHeight(), monitorH * 9 / 10);
    const Vector2 origin = GetMonitorPosition(monitor);
    SetWindowSize(w, h);
    SetWindowPosition(int(origin.x) + (monitorW - w) / 2, int(origin.y) + (monitorH - h) / 2);
  }
  if (settings.window.maximized) MaximizeWindow();
}

App::Window::~Window()
{
  ui::UnloadFonts();
  CloseWindow();
}

App::App()
{
  ApplySettings(this->settings);

  // The shortcuts are read before the Gui each frame; the keys they take
  // never reach it.
  this->gui.setKeyFilter([this](int key) { return this->shortcuts.claimed(key); });

  // A file double-clicked in Explorer arrives on the command line.
  const std::vector<std::filesystem::path> files = platform::CommandLineFiles();
  if (files.empty() || !this->open(files.front())) this->newGame();
}

int App::run()
{
  while (!this->quitRequested) this->frame();
  // Closed with the settings page still up: what it changed was never kept.
  if (this->gui.pages().current() == ui::Pages::Page::Settings) this->discardSettings();
  this->settings.save();  // for the window size, if nothing else
  return 0;
}

void App::frame()
{
  const float dt = GetFrameTime();

  // Shortcuts first, before the Gui hands the same keys to whatever has the
  // focus. Whether a text box has the caret is last frame's answer, which is
  // the one the player was looking at.
  const std::vector<Command> keys = this->shortcuts.poll(this->gui.editor().typing(), this->gui.pages().isOpen());

  // Settings next: a change made last frame -- a new interface scale, say --
  // is in place before the Gui lays itself out for this one.
  this->updateSettings();
  this->gui.update(dt);

  for (Command c : keys) this->handle(c);
  for (Command c : this->gui.editor().takeCommands()) this->handle(c);
  this->handlePages();
  this->handleDroppedFiles();
  this->handleClose();
  this->updateTitle();

  BeginDrawing();
  ClearBackground(cfg::BACKGROUND_COLOR);
  this->gui.draw();
  EndDrawing();
}

// ---------------------------------------------------------------- commands

void App::handle(Command command)
{
  ui::Pages& pages = this->gui.pages();

  switch (command) {
  case Command::NewGame:
    pages.newGame.refresh();
    pages.open(ui::Pages::Page::NewGame);
    break;
  case Command::Open:
    this->showFiles(false);
    break;
  case Command::Save:
    this->saveOrAsk();
    break;
  case Command::SaveAs:
    this->showFiles(true);
    break;
  case Command::GameInfo:
    if (this->game) pages.gameInfo.load(this->game->root());
    pages.open(ui::Pages::Page::GameInfo);
    break;
  case Command::Settings:
    this->kept = this->settings;
    pages.settings.open();
    pages.settings.setAssociation(platform::IsSgfAssociated() ? ".sgf files open in this program."
                                                              : ".sgf files open in something else, or nothing.",
                                  platform::IsSgfAssociated());
    pages.open(ui::Pages::Page::Settings);
    break;
  case Command::AiSensei:
    this->sendToAiSensei();
    break;
  case Command::Help:
    pages.open(ui::Pages::Page::Help);
    break;
  case Command::Cancel:
    // Esc on a page is its Back button.
    if (pages.isOpen()) {
      if (pages.current() == ui::Pages::Page::Settings) this->discardSettings();
      pages.close();
      this->after = After::None;
    } else {
      this->gui.editor().run(command);
    }
    break;
  default:
    this->gui.editor().run(command);
    break;
  }
}

void App::handlePages()
{
  ui::Pages& pages = this->gui.pages();

  switch (pages.takeAction()) {
  case ui::Pages::Action::StartGame:
    this->settings.save();  // so the next New game starts where this one did
    this->guard(After::NewGame);
    break;
  case ui::Pages::Action::ApplyGameInfo:
    if (this->game) this->game->setGameInfo(pages.gameInfo.values());
    break;
  case ui::Pages::Action::FileChosen:
    this->settings.folder = platform::ToUtf8(pages.files.folder());
    if (!this->saving) {
      this->guard(After::Open, pages.chosenFile());
    } else if (this->save(pages.chosenFile()) && this->after != After::None) {
      // Saved as part of answering "save the changes first?"
      this->proceed();
    }
    break;
  case ui::Pages::Action::Save:
    if (this->path.empty()) this->showFiles(true);  // then on to `after` once it's saved
    else if (this->save(this->path)) this->proceed();
    break;
  case ui::Pages::Action::Discard:
    this->proceed();
    break;
  case ui::Pages::Action::Associate: {
    std::string error;
    if (platform::AssociateSgfFiles(&error)) pages.settings.setAssociation("Done: double-click a .sgf file and it opens here.", true);
    else                                    pages.settings.setAssociation(error, false);
    break;
  }
  case ui::Pages::Action::SaveSettings:
    this->settings.save();
    break;
  case ui::Pages::Action::DiscardSettings:
    this->discardSettings();
    break;
  case ui::Pages::Action::Back:
    this->after = After::None;
    break;
  case ui::Pages::Action::None:
    break;
  }
}

void App::handleDroppedFiles()
{
  if (!IsFileDropped()) return;
  FilePathList files = LoadDroppedFiles();
  // GLFW hands the names over in UTF-8.
  const std::filesystem::path first = files.count > 0 ? platform::FromUtf8(files.paths[0]) : std::filesystem::path();
  UnloadDroppedFiles(files);
  if (!first.empty()) this->guard(After::Open, first);
}

void App::handleClose()
{
  if (!WindowShouldClose()) return;
  // Not yet: the answer to "save first?" decides. Cleared in GLFW, raylib
  // reads it again at the end of the frame.
  glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_FALSE);
  this->guard(After::Quit);
}

// ---------------------------------------------------------------- documents

void App::guard(After then, const std::filesystem::path& file)
{
  // Something else is taking over the screen: whatever the settings page
  // changed and didn't save goes, as if Back had been pressed.
  if (this->gui.pages().current() == ui::Pages::Page::Settings) {
    this->discardSettings();
    this->gui.pages().close();
  }
  this->after     = then;
  this->afterFile = file;
  if (this->game && this->game->modified()) {
    this->gui.pages().confirm.setFile(this->name());
    this->gui.pages().open(ui::Pages::Page::Confirm);
    return;
  }
  this->proceed();
}

void App::proceed()
{
  const After what = this->after;
  this->after = After::None;
  switch (what) {
  case After::NewGame: this->newGame(); break;
  case After::Open:    this->open(this->afterFile); break;
  case After::Quit:    this->quitRequested = true; break;
  case After::None:    break;
  }
}

void App::newGame()
{
  GameSetup setup = this->settings.newGame;
  setup.date = Today();
  this->game = std::make_unique<Game>(setup);
  this->path.clear();
  this->gui.editor().show(this->game.get());
}

bool App::open(const std::filesystem::path& file)
{
  ui::EditorView& editor = this->gui.editor();
  const std::string fileName = platform::ToUtf8(file.filename());

  std::string error;
  const std::optional<std::string> text = platform::ReadText(file, &error);
  if (!text) {
    editor.message(error, false);
    return false;
  }
  std::optional<sgf::Collection> parsed = sgf::Parse(*text, &error);
  if (!parsed) {
    editor.message("Couldn't read " + fileName + ": " + error + ".", false);
    return false;
  }
  const std::string gm = parsed->games.front()->get("GM");
  if (!gm.empty() && gm != "1") {
    editor.message(fileName + " is a record of some other game than Go (GM[" + gm + "]).", false);
    return false;
  }

  const size_t games = parsed->games.size();
  this->game = std::make_unique<Game>(std::move(*parsed));
  this->path = file;
  this->settings.folder = platform::ToUtf8(file.parent_path());
  editor.show(this->game.get());
  if (games > 1) editor.message(fileName + " holds " + std::to_string(games) + " games: this is the first. Saving keeps them all.");
  return true;
}

bool App::save(const std::filesystem::path& file)
{
  if (!this->game) return false;
  this->game->stampFormat();
  std::string error;
  if (!platform::WriteText(file, sgf::Write(this->game->file()), &error)) {
    this->gui.editor().message(error, false);
    return false;
  }
  this->path = file;
  this->game->markSaved();
  this->gui.editor().message("Saved " + platform::ToUtf8(file.filename()) + ".");
  return true;
}

void App::saveOrAsk()
{
  if (this->path.empty()) this->showFiles(true);
  else                    this->save(this->path);
}

void App::sendToAiSensei()
{
  if (!this->game) return;
  // What saving writes, CA[UTF-8] included, since that is how it's sent.
  this->game->stampFormat();
  const std::string sgf = sgf::Write(this->game->file());

  // ai-sensei.com/upload?sgf=... takes the record as pasted text: the upload
  // dialog opens with it, or after logging in if the browser isn't yet.
  // Percent-encoded byte by byte, so the UTF-8 of a name stays intact.
  std::string url = "https://ai-sensei.com/upload?sgf=";
  static const char HEX[] = "0123456789ABCDEF";
  for (const unsigned char c : sgf) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      url += char(c);
    } else {
      url += '%';
      url += HEX[c >> 4];
      url += HEX[c & 15];
    }
  }

  // Windows hands the URL to the browser on its command line, which holds
  // 32767 characters in all.
  constexpr size_t MAX_URL = 30000;
  if (url.size() > MAX_URL) {
    this->gui.editor().message("This game is too long to send this way: save it and upload the file on ai-sensei.com.",
                               false);
    return;
  }
  std::string error;
  if (platform::OpenInBrowser(url, &error)) this->gui.editor().message("Opened AI Sensei's upload page in the browser.");
  else                                      this->gui.editor().message(error, false);
}

void App::showFiles(bool forSaving)
{
  this->saving = forSaving;
  const std::string suggested = forSaving ? (this->path.empty() ? std::string("game.sgf") : platform::ToUtf8(this->path.filename()))
                                          : std::string();
  this->gui.pages().files.start(forSaving ? ui::FilePage::Mode::Save : ui::FilePage::Mode::Open, this->folder(), suggested);
  this->gui.pages().open(ui::Pages::Page::Files);
}

std::string App::name() const
{
  return this->path.empty() ? std::string("Untitled") : platform::ToUtf8(this->path.filename());
}

std::filesystem::path App::folder() const
{
  if (!this->path.empty()) return this->path.parent_path();
  if (!this->settings.folder.empty()) return platform::FromUtf8(this->settings.folder);
  return platform::DocumentsFolder();
}

// ---------------------------------------------------------------- the window

void App::updateTitle()
{
  const bool        changed = this->game && this->game->modified();
  const std::string title   = this->name() + (changed ? " *" : "");
  if (title == this->shownTitle) return;
  this->shownTitle = title;
  this->gui.editor().setTitle(title);
  SetWindowTitle((title + " - " + cfg::APP_NAME).c_str());
}

void App::discardSettings()
{
  this->settings.graphics = this->kept.graphics;
  this->settings.board    = this->kept.board;
  this->gui.pages().settings.refresh();
}

void App::updateSettings()
{
  // Ctrl and the numpad's + and - step the interface scale, from anywhere.
  if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
    const int steps = int(IsKeyPressed(KEY_KP_ADD)) - int(IsKeyPressed(KEY_KP_SUBTRACT));
    if (steps != 0) {
      int& scale = this->settings.graphics.interfaceScale;
      scale = ClampedInterfaceScale(scale + steps * Settings::Graphics::SCALE_STEP);
      this->gui.pages().settings.refresh();
      // Kept at once -- unless the settings page is up, where it is one more
      // change that Save or Back decides on.
      if (this->gui.pages().current() != ui::Pages::Page::Settings) this->settings.save();
    }
  }

  this->gui.editor().setBoardOptions({ this->settings.board.coordinates, this->settings.board.moveNumbers,
                                       this->settings.board.nextMoves });

  // The settings page edits `settings` directly; bring the window and the
  // Gui in line with whatever it changed.
  if (this->settings.graphics != this->applied.graphics || this->settings.board != this->applied.board) {
    if (this->settings.graphics.interfaceScale != this->applied.graphics.interfaceScale) {
      this->gui.setScale(this->settings.graphics.interfaceScale);
    }
    if (this->settings.graphics.tooltipDelay != this->applied.graphics.tooltipDelay) {
      this->gui.setTooltipDelay(this->settings.graphics.tooltipDelay);
    }
    ApplySettings(this->settings);
    this->applied = this->settings;
  }

  // Remember the window as the player left it. Not while windowed fullscreen,
  // which would record the monitor's size, nor maximized, which would record
  // the maximized size in place of the one to restore to, nor minimized, which
  // Windows reports as 0x0.
  if (!IsWindowedFullscreen() && !IsWindowMinimized()) {
    this->settings.window.maximized = IsWindowMaximized();
    if (!this->settings.window.maximized) {
      this->settings.window.width  = GetScreenWidth();
      this->settings.window.height = GetScreenHeight();
    }
  }
}
