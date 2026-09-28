// The Go board on screen. It is a widget tree like everything else here: a
// wooden panel, the grid laid on it as thin widgets, and over each point a
// button carrying pictures -- the stone, a mark -- and a label. So the points
// take clicks through the Gui, rather than by working out which point the
// mouse is over, and they theme and scale like the rest.
//
// It is also where a click means something. What it means depends on the
// tool (see Commands.hpp); with the Play tool:
//
//   click          plays a move, or steps into it if the record already has it
//   Ctrl+click     inserts the move after this one, the rest of the game
//                  following on after it, instead of starting a variation
//   drag a stone   moves it, and changes the move that played it -- however
//                  long ago that was
//   right-click or
//   Shift+click    on a stone, goes to the move that played it
//
// and the mouse wheel steps through the game.

#pragma once

#include <game/Game.hpp>
#include <ui/Commands.hpp>
#include <ui/GoSprites.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/MouseEvent.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/EmptyWidget.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ui {

class Theme;

// One point of the board: a see-through button with a stone, a mark and a
// label on it, in that order, so the label reads over the stone.
class PointWidget : public agui::Button {
  using super = agui::Button;

public:
  explicit PointWidget(const agui::ButtonStyle* style);

  // Each only does anything when asked for something other than what it
  // shows already -- which is what makes refreshing every point after every
  // move cheap enough to do.
  void setStone(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites);
  void setMark(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites);
  void setLabel(const std::string& text, const agui::LabelStyle* style);

protected:
  // The pictures are children, and a button does not lay its children out,
  // so they are kept over the point by hand.
  void onSizeChanged(agui::Dimension originalSize) override;

private:
  void place();

  agui::ImageWidget stone;
  agui::ImageWidget mark;
  agui::Label       label;

  std::optional<Sprite>   shownStone;
  std::optional<Sprite>   shownMark;
  double                  stoneOpacity = 1.0;
  double                  markOpacity  = 1.0;
  const agui::LabelStyle* labelStyle   = nullptr;
};

// SGF's arrows and lines, which run from point to point across the board and
// so can't be a picture on either. A see-through layer over the points that
// draws them with the Gui's own line drawing.
class LineLayer : public agui::Widget {
public:
  struct Line {
    agui::Point from, to;
    bool        arrow;
  };
  LineLayer();
  void setLines(std::vector<Line> lines, float thickness);

protected:
  void paintComponent(const agui::PaintEvent& paintEvent, const agui::Point& absolutePosition) override;

private:
  std::vector<Line> lines;
  float             thickness = 2.0f;
};

class BoardView : public agui::GenericTargetable {
public:
  struct Options {
    bool coordinates = true;
    bool moveNumbers = false;  // on every stone, not just the last
    bool nextMoves   = true;   // where the variations from here go, as faint stones

    bool operator==(const Options&) const = default;
  };

  BoardView(Theme& theme, const GoSprites& sprites);
  ~BoardView();
  BoardView(const BoardView&) = delete;
  BoardView& operator=(const BoardView&) = delete;

  // The panel, for whoever lays the screen out to add to the Gui.
  agui::EmptyWidget& widget() { return this->board; }

  // The game to show and edit, or null for none. Not owned.
  void show(Game* game);
  void setOptions(const Options& options);
  // The tool clicks use; `text` is what the Text tool writes.
  void setTool(Tool tool, const std::string& text);
  // Drops a half-drawn arrow or line.
  void cancel();

  // Brings the points in line with the game. Cheap when nothing changed, so
  // it is called every frame.
  void refresh();

  // Fits the board, as big as it will go, into this rectangle of the screen.
  void layout(int x, int y, int width, int height);

  // The interface scale, in percent. At one like 125% a GUI unit is not a
  // whole number of pixels, and a one-unit line would come out one pixel
  // wide or two depending on where it fell. So the board keeps every line on
  // a GUI coordinate that is a whole pixel on screen, and they all come out
  // alike.
  void setScale(int percent);

  // While a page is up, the board shows but takes no clicks.
  void setInteractive(bool value);

  // What a click came to, for the status line; and the mouse wheel, as moves
  // to step (negative is back).
  std::function<void(const Outcome&)> onOutcome;
  std::function<void(int)>            onWheel;

private:
  void build();
  void place();

  void pressed(Point p, const agui::MouseEvent& event);
  void dragged(Point p, const agui::MouseEvent& event);
  void released(Point p, const agui::MouseEvent& event);
  void click(Point p, bool ctrl, bool shift);
  void rightClick(Point p);
  // The point under the mouse, from an event on the point it was pressed on:
  // everything after the press goes to that point, wherever the mouse is.
  Point pointAt(Point pressedOn, const agui::MouseEvent& event) const;

  void report(const Outcome& outcome);

  PointWidget& point(Point p) { return *this->points[size_t(p.y) * size_t(this->columns) + size_t(p.x)]; }

  Theme&           theme;
  const GoSprites& sprites;

  // The only widget the view owns outright; everything under it is held by
  // the tree and goes with it.
  agui::EmptyWidget board;

  std::vector<PointWidget*>       points;  // row by row
  std::vector<agui::EmptyWidget*> lines;   // columns, then rows
  std::vector<agui::ImageWidget*> stars;
  std::vector<Point>              starPoints;
  std::vector<agui::Label*>       columnLabels;  // top then bottom, per column
  std::vector<agui::Label*>       rowLabels;     // left then right, per row
  LineLayer*                      lineLayer = nullptr;

  Game*   game = nullptr;
  Options options;
  Tool    tool = Tool::Play;
  std::string text;

  int columns = 0, rows = 0;
  int pitch = 0, margin = 0;  // px per point, and round the grid
  int snap  = 1;              // GUI units to a whole screen pixel: see setScale()

  // How far a line `width` wide starts before the middle of its points:
  // half its width, but on the pixel grid.
  int offGrid(int width) const { return width / 2 / this->snap * this->snap; }

  Point hover;       // the point under the mouse
  Point pressPoint;  // where the left button went down
  Point dragTarget;  // where a dragged stone would go
  bool  dragging = false;
  Point lineStart;   // the first click of an arrow or a line

  uint64_t shownRevision = 0;
  bool     dirty         = true;
};

}  // namespace ui
