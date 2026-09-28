// The About page: who made the editor, where it lives, and how it came about.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>

namespace ui {

class Theme;

// Where the editor's source is.
inline constexpr const char* PROJECT_URL = "https://github.com/kovarex/kovarex_go_editor";

class AboutPage : public agui::GenericTargetable {
public:
  // `onLink` opens PROJECT_URL -- the App's to do, with the browser.
  AboutPage(Theme& theme, std::function<void()> onLink, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

private:
  agui::Window window;
};

}  // namespace ui
