#include <ui/GuiLayer.hpp>

#include <app/Settings.hpp>
#include <ui/EditorView.hpp>
#include <ui/GoSprites.hpp>
#include <ui/Pages.hpp>
#include <ui/Theme.hpp>

#include <Agui/Gui.hpp>
#include <rlgl.h>

namespace ui {

GuiLayer::GuiLayer(Settings& settings)
{
  // Theme loads fonts through Agui, so the loader has to be in place first.
  agui::Font::setFontLoader(&this->fontLoader);
  this->theme   = std::make_unique<Theme>();
  this->sprites = std::make_unique<GoSprites>();

  this->gui = std::make_unique<agui::Gui>();
  this->gui->setGraphics(&this->graphics);
  this->gui->setInput(&this->input);
  this->gui->setCursorProvider(&this->cursor);
  this->gui->resizeToDisplay();

  // Order is depth: the editor at the back, the pages (and the sheet they dim
  // the editor with) on top.
  this->editorView = std::make_unique<EditorView>(*this->gui, *this->theme, *this->sprites);
  this->pageStack  = std::make_unique<Pages>(*this->gui, *this->theme, *this->sprites, settings);

  this->setScale(settings.graphics.interfaceScale);
  this->setTooltipDelay(settings.graphics.tooltipDelay);
}

void GuiLayer::setTooltipDelay(int milliseconds)
{
  // Agui's own "never" is -1 seconds.
  this->gui->setGuiTooltipHoverInterval(milliseconds < 0 ? -1.0 : double(milliseconds) / 1000.0);
  this->gui->resetGuiTooltipHoverTime();
}

GuiLayer::~GuiLayer()
{
  this->pageStack.reset();
  this->editorView.reset();
  this->gui.reset();
  this->sprites.reset();
  this->theme.reset();
}

void GuiLayer::setScale(int percent)
{
  const float scale = float(percent) / 100.0f;
  this->graphics.setViewScale(scale);
  this->input.setViewScale(scale);
  this->theme->setScale(scale);
  this->editorView->setScale(percent);
}

void GuiLayer::setKeyFilter(std::function<bool(int key)> filter)
{
  this->input.setKeyFilter(std::move(filter));
}

void GuiLayer::update(float dt)
{
  // Compared rather than asking IsWindowResized(): a resize made mid-frame, like
  // toggling fullscreen from the settings page, is cleared by the next
  // EndDrawing() before this ever sees it. In GUI units, so a change of scale
  // is a resize too. Not while minimized: Windows reports that as 0x0, and
  // laying out for it would recentre everything the player had dragged
  // somewhere.
  const agui::Dimension display = this->graphics.getDisplaySize();
  if (!IsWindowMinimized() && (display.width != this->screenWidth || display.height != this->screenHeight)) {
    this->screenWidth  = display.width;
    this->screenHeight = display.height;
    this->gui->resizeToDisplay();
  }
  // Shift shows tooltips at once, whatever the delay -- even when it is
  // "never". Agui takes back the ones it showed when Shift is let go of.
  this->gui->setInstantTooltip(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
  this->gui->logic(true);

  // The editor is still there behind a page, dimmed, but nothing on it can be
  // clicked.
  this->editorView->setInteractive(!this->pageStack->isOpen());
  this->editorView->update(dt, this->screenWidth, this->screenHeight);
  this->pageStack->layout(this->screenWidth, this->screenHeight);
}

void GuiLayer::draw()
{
  // Agui draws in GUI units; the matrix turns them into screen pixels.
  rlPushMatrix();
  rlScalef(this->graphics.getViewScale(), this->graphics.getViewScale(), 1.0f);
  this->graphics._beginPaint();
  this->gui->render();
  this->graphics._endPaint();
  rlPopMatrix();
}

}  // namespace ui
