// The file browser, for Open and Save as: a folder's sub-folders and .sgf
// files in a list, the drives to jump between, and a name to type. Built from
// Agui widgets like every other page, rather than Windows' own dialog.
//
// Double-click a folder to go in, a file to open it; ".." goes up. A full
// path typed into the name box works too.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace agui {
class Button;
class DropDown;
class Label;
class ListBox;
class TextField;
}  // namespace agui

namespace ui {

class Theme;

class FilePage : public agui::GenericTargetable {
public:
  enum class Mode { Open, Save };

  FilePage(Theme& theme, std::function<void(const std::filesystem::path&)> onChosen, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // Opens in `folder`, with `name` in the name box.
  void start(Mode mode, const std::filesystem::path& folder, const std::string& name);

  // Where the browser is now, to start from there next time.
  const std::filesystem::path& folder() const { return this->current; }

private:
  struct Entry {
    std::filesystem::path path;
    bool                  folder;
  };

  void enter(const std::filesystem::path& folder);
  void picked(int index);
  void opened(int index);
  void confirm();

  Theme&       theme;
  agui::Window window;
  std::function<void(const std::filesystem::path&)> onChosen;

  Mode                  mode = Mode::Open;
  std::filesystem::path current;
  std::vector<Entry>    entries;
  std::vector<std::filesystem::path> drives;

  agui::Label*     where   = nullptr;
  agui::DropDown*  drive   = nullptr;
  agui::ListBox*   list    = nullptr;
  agui::TextField* name    = nullptr;
  agui::Button*    confirmButton = nullptr;
  agui::Label*     problem = nullptr;
};

}  // namespace ui
