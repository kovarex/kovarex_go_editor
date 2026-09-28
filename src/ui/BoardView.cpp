#include <ui/BoardView.hpp>

#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Graphics.hpp>
#include <Agui/Gui.hpp>
#include <Agui/PaintEvent.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ui {

namespace {

// Round the grid, in points: room for the coordinates, or just a rim of wood.
constexpr float MARGIN_WITH_COORDINATES = 0.9f;
constexpr float MARGIN_WITHOUT          = 0.4f;

// How see-through the stones that aren't really there are.
constexpr double GHOST   = 0.5;   // the stone a click would play
constexpr double HINT    = 0.35;  // where the variations from here go
constexpr double DIMMED  = 0.45;  // DD: a stone the diagram wants in the background
constexpr double LIFTED  = 0.3;   // a stone being dragged away from its point

int FloorDiv(int a, int b)
{
  return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// A white stone is one of the shells, the one this point always has.
Sprite StoneSprite(Stone s, Point p)
{
  return s == Stone::Black ? Sprite::BlackStone : WhiteShell(unsigned(p.x) * 73856093u ^ unsigned(p.y) * 19349663u);
}

// A mark in the colour that shows on what is under it.
Sprite MarkSprite(Mark mark, Stone under)
{
  const int light = under == Stone::Black ? 1 : 0;
  switch (mark) {
  case Mark::Triangle:       return Sprite(int(Sprite::TriangleDark) + light);
  case Mark::Square:         return Sprite(int(Sprite::SquareDark) + light);
  case Mark::Circle:         return Sprite(int(Sprite::CircleDark) + light);
  case Mark::Cross:          return Sprite(int(Sprite::CrossDark) + light);
  case Mark::Selected:       return Sprite::Selected;
  case Mark::TerritoryBlack: return Sprite::TerritoryBlack;
  case Mark::TerritoryWhite: return Sprite::TerritoryWhite;
  case Mark::Dim:            break;
  }
  return Sprite::Selected;
}

std::optional<Mark> ToolMark(Tool tool)
{
  switch (tool) {
  case Tool::Triangle:       return Mark::Triangle;
  case Tool::Square:         return Mark::Square;
  case Tool::Circle:         return Mark::Circle;
  case Tool::Cross:          return Mark::Cross;
  case Tool::Selected:       return Mark::Selected;
  case Tool::TerritoryBlack: return Mark::TerritoryBlack;
  case Tool::TerritoryWhite: return Mark::TerritoryWhite;
  case Tool::Dim:            return Mark::Dim;
  default:                   return std::nullopt;
  }
}

// Sizes a widget exactly, style and all, so the layout keeps it that size.
void Pin(agui::Widget& widget, agui::Style& style, int w, int h)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
  style.setMinimalHeight(h);
  style.setMaximalHeight(h);
  widget.setSize(w, h, agui::SetSizeInfo());
}

}  // namespace

// ---------------------------------------------------------------- point

PointWidget::PointWidget(const agui::ButtonStyle* style)
    : agui::Button(style)
{
  this->mouseButtonFilter = agui::MouseButton::ALL;
  // A point is pressed and released, dragged from and dropped on; none of
  // that is a click, which a button would otherwise fire on the press.
  this->disableFireClickOnMouseDown();
  this->setFocusable(false);

  for (agui::ImageWidget* image : { &this->patch, &this->shadow, &this->stone, &this->mark }) {
    image->scaleToKeepTheRatio = image != &this->patch;
    image->scale               = image == &this->patch;  // its piece of the wood, stretched as the board's is
    image->setIgnoredByInteraction(true);
    image->setVisible(false);
    this->add(image);
  }
  this->label.setIgnoredByInteraction(true);
  this->label.setVisible(false);
  this->add(&this->label);
}

void PointWidget::setStone(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites)
{
  if (sprite == this->shownStone && opacity == this->stoneOpacity) return;
  if (sprite && sprite != this->shownStone) this->stone.setImage(sprites.image(*sprite));
  this->shownStone    = sprite;
  this->stoneOpacity  = opacity;
  this->stone.opacity = opacity;
  this->stone.setVisible(sprite.has_value());
  // A stone that is really there casts a shadow; a faint one -- where a
  // variation goes, where the mouse would play -- doesn't.
  const bool casts = sprite.has_value() && opacity > 0.5;
  if (casts && this->shadow.getImage() == nullptr) this->shadow.setImage(sprites.image(Sprite::StoneShadow));
  this->shadow.opacity = opacity;
  this->shadow.setVisible(casts);
  this->place();
}

void PointWidget::setMark(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites)
{
  if (sprite == this->shownMark && opacity == this->markOpacity) return;
  if (sprite && sprite != this->shownMark) this->mark.setImage(sprites.image(*sprite));
  this->shownMark    = sprite;
  this->markOpacity  = opacity;
  this->mark.opacity = opacity;
  this->mark.setVisible(sprite.has_value());
  this->place();
}

void PointWidget::setLabel(const std::string& value, const agui::LabelStyle* look)
{
  if (value == this->label.getText() && (value.empty() || look == this->labelStyle)) return;
  if (look && look != this->labelStyle) this->label.style.setParent(look);
  this->labelStyle = look;
  this->label.setText(value);
  this->label.setVisible(!value.empty());
  this->place();
}

void PointWidget::setWood(std::unique_ptr<agui::Image> image)
{
  this->patch.setImage(std::move(image));
}

void PointWidget::setBare(bool bare)
{
  if (bare == this->patch.isVisible()) return;
  this->patch.setVisible(bare);
  this->place();
}

void PointWidget::onSizeChanged(agui::Dimension originalSize)
{
  super::onSizeChanged(originalSize);
  this->place();
}

void PointWidget::place()
{
  const int side = std::min(this->getWidth(), this->getHeight());
  if (side <= 0) return;
  for (agui::ImageWidget* image : { &this->patch, &this->stone, &this->mark }) {
    if (!image->isVisible()) continue;
    Pin(*image, image->style, side, side);
    image->setLocation(0, 0);
  }
  if (this->shadow.isVisible()) {
    const int px = int(std::lround(float(side) * SHADOW_SCALE));
    Pin(this->shadow, this->shadow.style, px, px);
    this->shadow.setLocation((side - px) / 2 + side * 7 / 100, (side - px) / 2 + side * 9 / 100);
  }
  if (this->label.isVisible()) {
    this->label.style.setMinimalWidth(side);
    this->label.style.setMaximalWidth(side);
    this->label.resizeToContents();
    this->label.setLocation(0, (side - this->label.getHeight()) / 2);
  }
}

// ---------------------------------------------------------------- lines

LineLayer::LineLayer()
{
  this->setIgnoredByInteraction(true);
  this->setFocusable(false);
}

void LineLayer::setLines(std::vector<Line> newLines, float newThickness)
{
  this->lines     = std::move(newLines);
  this->thickness = newThickness;
}

void LineLayer::paintComponent(const agui::PaintEvent& paintEvent, const agui::Point&)
{
  const agui::Color ink(0.95f, 0.35f, 0.15f, 0.9f);
  for (const Line& line : this->lines) {
    paintEvent.graphics()->drawLine(line.from, line.to, ink, this->thickness);
    if (!line.arrow) continue;

    // A head at the far end, as long as the line is thick, three times over.
    const float dx = float(line.to.x - line.from.x);
    const float dy = float(line.to.y - line.from.y);
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) continue;
    const float ux = dx / length, uy = dy / length;
    const float head = this->thickness * 4.0f;
    const float bx = float(line.to.x) - ux * head, by = float(line.to.y) - uy * head;
    paintEvent.graphics()->drawFilledTriangle(
        line.to, agui::Point(int(bx - uy * head * 0.6f), int(by + ux * head * 0.6f)),
        agui::Point(int(bx + uy * head * 0.6f), int(by - ux * head * 0.6f)), ink);
  }
}

// ---------------------------------------------------------------- board

BoardView::BoardView(Theme& theme, const GoSprites& sprites)
    : theme(theme)
    , sprites(sprites)
    , board(&theme.boardWood)
{
  this->board.setVisible(false);
}

BoardView::~BoardView() = default;

void BoardView::show(Game* newGame)
{
  this->game = newGame;
  this->hover = this->pressPoint = this->dragTarget = this->lineStart = {};
  this->dragging = false;
  this->board.setVisible(newGame != nullptr);
  if (newGame && (newGame->width() != this->columns || newGame->height() != this->rows)) this->build();
  this->dirty = true;
}

void BoardView::setOptions(const Options& newOptions)
{
  if (newOptions == this->options) return;
  const bool relayout = newOptions.coordinates != this->options.coordinates;
  this->options = newOptions;
  if (relayout) this->pitch = 0;  // the margin changes, so everything moves
  this->dirty = true;
}

void BoardView::setTool(Tool newTool, const std::string& newText)
{
  if (newTool != this->tool) this->lineStart = {};
  this->tool  = newTool;
  this->text  = newText;
  this->dirty = true;
}

void BoardView::cancel()
{
  this->lineStart = {};
  this->dirty     = true;
}

void BoardView::setInteractive(bool value)
{
  this->board.setIgnoredByInteraction(!value);
  if (!value && this->hover.valid()) {
    this->hover = {};
    this->dirty = true;
  }
}

void BoardView::build()
{
  this->board.clear();
  this->points.clear();
  this->lines.clear();
  this->stars.clear();
  this->columnLabels.clear();
  this->rowLabels.clear();

  this->columns = this->game->width();
  this->rows    = this->game->height();
  this->pitch   = 0;  // place() on the next layout

  // In drawing order: the wood's grain, the grid, the star points and the
  // coordinates on it, the points over them, and the arrows over everything.
  this->wood = &make<agui::ImageWidget>(this->sprites.wood(), true);
  this->wood->setIgnoredByInteraction(true);
  this->board << *this->wood;
  for (int i = 0; i < this->columns + this->rows; ++i) {
    agui::EmptyWidget& line = make<agui::EmptyWidget>(&this->theme.gridLine);
    line.setIgnoredByInteraction(true);
    this->board << line;
    this->lines.push_back(&line);
  }
  this->starPoints = StarPoints(this->columns, this->rows);
  for (size_t i = 0; i < this->starPoints.size(); ++i) {
    agui::ImageWidget& star = make<agui::ImageWidget>(this->sprites.image(Sprite::StarPoint));
    star.scaleToKeepTheRatio = true;
    star.setIgnoredByInteraction(true);
    this->board << star;
    this->stars.push_back(&star);
  }
  for (int x = 0; x < this->columns; ++x) {
    for (int side = 0; side < 2; ++side) {
      agui::Label& label = make<agui::Label>(ColumnName(x, this->columns), &this->theme.coordinate);
      label.setIgnoredByInteraction(true);
      this->board << label;
      this->columnLabels.push_back(&label);
    }
  }
  for (int y = 0; y < this->rows; ++y) {
    for (int side = 0; side < 2; ++side) {
      agui::Label& label = make<agui::Label>(RowName(y, this->rows), &this->theme.coordinate);
      label.setIgnoredByInteraction(true);
      this->board << label;
      this->rowLabels.push_back(&label);
    }
  }

  this->points.reserve(size_t(this->columns) * size_t(this->rows));
  for (int y = 0; y < this->rows; ++y) {
    for (int x = 0; x < this->columns; ++x) {
      const Point p{ x, y };
      PointWidget& point = make<PointWidget>(&this->theme.pointPlain);
      point.onMouseDown(this, [this, p](const agui::MouseEvent& e) { this->pressed(p, e); });
      point.onMouseDrag(this, [this, p](const agui::MouseEvent& e) { this->dragged(p, e); });
      point.onMouseUp(this, [this, p](const agui::MouseEvent& e) { this->released(p, e); });
      point.onMouseEnter(this, [this, p](const agui::MouseEvent&) {
        this->hover = p;
        this->dirty = true;
      });
      point.onMouseLeave(this, [this, p](const agui::MouseEvent&) {
        if (this->hover == p) this->hover = {};
        this->dirty = true;
      });
      point.onMouseWheelUp(this, [this](const agui::MouseEvent&) { if (this->onWheel) this->onWheel(-1); });
      point.onMouseWheelDown(this, [this](const agui::MouseEvent&) { if (this->onWheel) this->onWheel(1); });
      this->board << point;
      this->points.push_back(&point);
    }
  }

  this->lineLayer = &make<LineLayer>();
  this->board << *this->lineLayer;
}

// ---------------------------------------------------------------- layout

void BoardView::layout(int x, int y, int width, int height)
{
  if (!this->game || this->columns == 0) return;

  const float marginPoints = this->options.coordinates ? MARGIN_WITH_COORDINATES : MARGIN_WITHOUT;
  const float across       = float(this->columns) + 2.0f * marginPoints;
  const float down         = float(this->rows) + 2.0f * marginPoints;
  // Everything on the pixel grid (see setScale()): the pitch a whole number
  // of steps either side of a point's middle, the margin whole steps.
  const int step     = this->snap;
  const int fit      = int(std::min(float(width) / across, float(height) / down));
  const int newPitch = std::max(2 * step * ((8 + 2 * step - 1) / (2 * step)), fit / (2 * step) * (2 * step));

  if (newPitch != this->pitch) {
    this->pitch  = newPitch;
    this->margin = std::max(step, int(std::lround(float(newPitch) * marginPoints / float(step))) * step);
    this->theme.setPointSize(newPitch);
    this->place();
  }

  const int boardW = this->pitch * this->columns + 2 * this->margin;
  const int boardH = this->pitch * this->rows + 2 * this->margin;
  const int left = (x + (width - boardW) / 2) / step * step;
  const int top  = (y + (height - boardH) / 2 + step - 1) / step * step;
  this->board.setLocation(left, top);
}

void BoardView::setScale(int percent)
{
  // The smallest number of GUI units that is a whole number of screen pixels
  // at this scale: 4 at 125% (5 px), 2 at 150% (3 px), 1 at 100% or 200%.
  const int newSnap = 100 / std::gcd(std::max(1, percent), 100);
  if (newSnap == this->snap) return;
  this->snap  = newSnap;
  this->pitch = 0;  // placed again on the next layout
}

void BoardView::place()
{
  const int boardW = this->pitch * this->columns + 2 * this->margin;
  const int boardH = this->pitch * this->rows + 2 * this->margin;
  Pin(this->board, this->board.style, boardW, boardH);
  Pin(*this->wood, this->wood->style, boardW, boardH);
  this->wood->setLocation(0, 0);

  // The grid runs through the middle of the points. The edge lines are
  // drawn heavier, as on a real board.
  const int thin  = std::max(1, this->pitch / 30);
  const int thick = thin + std::max(1, this->pitch / 40);
  const int half  = this->pitch / 2;
  const int x0    = this->margin + half;
  const int y0    = this->margin + half;
  const int spanX = (this->columns - 1) * this->pitch;
  const int spanY = (this->rows - 1) * this->pitch;
  for (int i = 0; i < this->columns; ++i) {
    agui::EmptyWidget& line = *this->lines[size_t(i)];
    const int w = (i == 0 || i == this->columns - 1) ? thick : thin;
    Pin(line, line.style, w, spanY + thick);
    line.setLocation(x0 + i * this->pitch - this->offGrid(w), y0 - this->offGrid(thick));
  }
  for (int j = 0; j < this->rows; ++j) {
    agui::EmptyWidget& line = *this->lines[size_t(this->columns + j)];
    const int h = (j == 0 || j == this->rows - 1) ? thick : thin;
    Pin(line, line.style, spanX + thick, h);
    line.setLocation(x0 - this->offGrid(thick), y0 + j * this->pitch - this->offGrid(h));
  }

  for (size_t i = 0; i < this->stars.size(); ++i) {
    agui::ImageWidget& star = *this->stars[i];
    Pin(star, star.style, this->pitch, this->pitch);
    star.setLocation(this->margin + this->starPoints[i].x * this->pitch, this->margin + this->starPoints[i].y * this->pitch);
  }

  // The coordinates, centred in the margin on each side.
  for (agui::Label* label : this->columnLabels) {
    label->setVisible(this->options.coordinates);
    label->style.setMinimalWidth(this->pitch);
    label->style.setMaximalWidth(this->pitch);
    label->resizeToContents();
  }
  for (agui::Label* label : this->rowLabels) {
    label->setVisible(this->options.coordinates);
    label->style.setMinimalWidth(this->margin);
    label->style.setMaximalWidth(this->margin);
    label->resizeToContents();
  }
  if (this->options.coordinates) {
    for (int i = 0; i < this->columns; ++i) {
      agui::Label& top    = *this->columnLabels[size_t(i) * 2];
      agui::Label& bottom = *this->columnLabels[size_t(i) * 2 + 1];
      const int    lx     = this->margin + i * this->pitch;
      top.setLocation(lx, (this->margin - top.getHeight()) / 2);
      bottom.setLocation(lx, boardH - this->margin + (this->margin - bottom.getHeight()) / 2);
    }
    for (int j = 0; j < this->rows; ++j) {
      agui::Label& left  = *this->rowLabels[size_t(j) * 2];
      agui::Label& right = *this->rowLabels[size_t(j) * 2 + 1];
      const int    ly    = this->margin + j * this->pitch + (this->pitch - left.getHeight()) / 2;
      left.setLocation(0, ly);
      right.setLocation(boardW - this->margin, ly);
    }
  }

  for (int j = 0; j < this->rows; ++j) {
    for (int i = 0; i < this->columns; ++i) {
      PointWidget& p = this->point({ i, j });
      p.triggerResize();
      const int x = this->margin + i * this->pitch, y = this->margin + j * this->pitch;
      p.setLocation(x, y);
      p.setWood(this->sprites.wood(float(x) / float(boardW), float(y) / float(boardH), float(x + this->pitch) / float(boardW),
                                   float(y + this->pitch) / float(boardH)));
    }
  }

  this->lineLayer->setLocation(0, 0);
  this->lineLayer->setSize(boardW, boardH);
  this->dirty = true;
}

// ---------------------------------------------------------------- showing the game

void BoardView::refresh()
{
  if (!this->game) return;
  // Turning a rectangular board, or undoing that, swaps its sides.
  if (this->game->width() != this->columns || this->game->height() != this->rows) {
    this->build();
    this->dirty = true;
  }
  if (this->points.empty()) return;
  if (!this->dirty && this->shownRevision == this->game->revision()) return;
  this->dirty         = false;
  this->shownRevision = this->game->revision();

  const Position&  position = this->game->position();
  const sgf::Node& node     = this->game->current();
  const size_t     count    = this->points.size();
  const auto       at       = [this](Point p) { return size_t(p.y) * size_t(this->columns) + size_t(p.x); };

  // What the current node marks, point by point.
  std::vector<std::optional<Mark>> marks(count);
  std::vector<std::string>         labels(count);
  std::vector<bool>                dim(count, false);
  for (Mark mark : { Mark::TerritoryBlack, Mark::TerritoryWhite, Mark::Selected, Mark::Triangle, Mark::Square,
                     Mark::Circle, Mark::Cross }) {
    // Shapes last, so a shape wins over territory on the same point.
    for (Point p : PointList(node.values(MarkProperty(mark)))) {
      if (position.inside(p)) marks[at(p)] = mark;
    }
  }
  for (Point p : PointList(node.values("DD"))) {
    if (position.inside(p)) dim[at(p)] = true;
  }
  for (const std::string& lb : node.values("LB")) {
    const std::optional<Point> p = lb.size() >= 3 && lb[2] == ':' ? FromSgf(lb.substr(0, 2)) : std::nullopt;
    if (p && position.inside(*p)) labels[at(*p)] = lb.substr(3);
  }

  // Faint stones: where the variations from here go, the stone a click would
  // play, and a stone being dragged.
  std::vector<std::optional<Stone>> ghosts(count);
  std::vector<double>               ghostOpacity(count, GHOST);
  if (this->options.nextMoves && node.childCount() > 1) {
    for (size_t k = 0; k < node.childCount(); ++k) {
      const Point p = this->game->movePoint(node.child(k));
      if (!p.valid() || position.at(p) != Stone::None) continue;
      ghosts[at(p)]       = Game::MoveColor(node.child(k));
      ghostOpacity[at(p)] = HINT;
    }
  }
  if (this->dragging && this->dragTarget.valid() && position.inside(this->dragTarget) &&
      position.at(this->dragTarget) == Stone::None) {
    ghosts[at(this->dragTarget)]       = position.at(this->pressPoint);
    ghostOpacity[at(this->dragTarget)] = GHOST;
  } else if (this->hover.valid() && !this->dragging && position.inside(this->hover) && position.at(this->hover) == Stone::None) {
    std::optional<Stone> preview;
    if (this->tool == Tool::Play)  preview = this->game->toPlay();
    if (this->tool == Tool::Black) preview = Stone::Black;
    if (this->tool == Tool::White) preview = Stone::White;
    if (preview) {
      ghosts[at(this->hover)]       = preview;
      ghostOpacity[at(this->hover)] = GHOST;
    }
  }

  const Point last = Game::MoveColor(node) != Stone::None ? this->game->movePoint(node) : Point{};
  const std::optional<Mark> toolMark = ToolMark(this->tool);

  for (int y = 0; y < this->rows; ++y) {
    for (int x = 0; x < this->columns; ++x) {
      const Point  p     = { x, y };
      const size_t i     = at(p);
      const Stone  stone = position.at(p);
      PointWidget& w     = this->point(p);

      // The stone, real or faint.
      std::optional<Sprite> stoneSprite;
      double                stoneOpacity = 1.0;
      if (stone != Stone::None) {
        stoneSprite  = StoneSprite(stone, p);
        stoneOpacity = dim[i] ? DIMMED : 1.0;
        if (this->dragging && p == this->pressPoint && this->dragTarget.valid() && this->dragTarget != p) stoneOpacity = LIFTED;
      } else if (ghosts[i]) {
        stoneSprite  = StoneSprite(*ghosts[i], p);
        stoneOpacity = ghostOpacity[i];
      }
      w.setStone(stoneSprite, stoneOpacity, this->sprites);

      // The label: the node's own, else the move number when they are shown.
      std::string label = labels[i];
      if (label.empty() && this->options.moveNumbers && stone != Stone::None && position.moveNumberAt(p) > 0) {
        label = std::to_string(position.moveNumberAt(p));
      }
      const bool  onBlack = stone == Stone::Black;
      const bool  small   = label.size() >= 3;
      const agui::LabelStyle* labelStyle = onBlack ? (small ? &this->theme.pointLightSmall : &this->theme.pointLight)
                                                   : (small ? &this->theme.pointDarkSmall : &this->theme.pointDark);
      w.setLabel(label, labelStyle);
      w.setBare(!label.empty() && stone == Stone::None);

      // The mark, or the last move's ring, or the mark the tool would put down.
      std::optional<Sprite> markSprite;
      double                markOpacity = 1.0;
      if (marks[i]) {
        markSprite = MarkSprite(*marks[i], stone);
      } else if (p == this->lineStart) {
        markSprite = Sprite::Selected;
      } else if (p == last && label.empty()) {
        // A ring on the stone, as CGoban has it: dark on white, light on black.
        markSprite = MarkSprite(Mark::Circle, stone);
      } else if (p == this->hover && toolMark && *toolMark != Mark::Dim && label.empty()) {
        markSprite  = MarkSprite(*toolMark, stone);
        markOpacity = GHOST;
      }
      w.setMark(markSprite, markOpacity, this->sprites);
    }
  }

  // Arrows and lines, from the middle of one point to the middle of another.
  std::vector<LineLayer::Line> drawn;
  const auto centre = [this](Point p) {
    return agui::Point(this->margin + p.x * this->pitch + this->pitch / 2, this->margin + p.y * this->pitch + this->pitch / 2);
  };
  for (const char* id : { "AR", "LN" }) {
    for (const std::string& v : node.values(id)) {
      if (v.size() != 5 || v[2] != ':') continue;
      const std::optional<Point> a = FromSgf(v.substr(0, 2));
      const std::optional<Point> b = FromSgf(v.substr(3, 2));
      if (!a || !b || !position.inside(*a) || !position.inside(*b)) continue;
      drawn.push_back({ centre(*a), centre(*b), id[0] == 'A' });
    }
  }
  this->lineLayer->setLines(std::move(drawn), std::max(2.0f, float(this->pitch) / 12.0f));
}

// ---------------------------------------------------------------- the mouse

Point BoardView::pointAt(Point pressedOn, const agui::MouseEvent& event) const
{
  if (this->pitch <= 0) return {};
  const Point p{ pressedOn.x + FloorDiv(event.getPosition().x, this->pitch),
                 pressedOn.y + FloorDiv(event.getPosition().y, this->pitch) };
  if (p.x < 0 || p.y < 0 || p.x >= this->columns || p.y >= this->rows) return {};
  return p;
}

void BoardView::pressed(Point p, const agui::MouseEvent& event)
{
  // Clicking the board lets go of a text box, so the keys are shortcuts again.
  if (agui::Gui* gui = this->board.getGui()) {
    if (agui::Widget* focused = gui->getFocusedWidget(); focused && focused->isTextBox()) gui->clearFocus();
  }
  if (event.getButton() != agui::MouseButton::LEFT) return;
  this->pressPoint = p;
  this->dragTarget = p;
  this->dragging   = false;
}

void BoardView::dragged(Point p, const agui::MouseEvent& event)
{
  if (!this->game || this->pressPoint != p) return;
  // Only stones move, and only with the tools that put stones down.
  const bool stoneTool = this->tool == Tool::Play || this->tool == Tool::Black || this->tool == Tool::White;
  if (!stoneTool || this->game->position().at(p) == Stone::None) return;

  const Point target = this->pointAt(p, event);
  if (target != this->dragTarget || !this->dragging) {
    this->dragTarget = target;
    this->dragging   = target != p;
    this->dirty      = true;
  }
}

void BoardView::released(Point p, const agui::MouseEvent& event)
{
  if (!this->game) return;

  if (event.getButton() == agui::MouseButton::RIGHT) {
    // Released where it was pressed, like a click.
    if (this->pointAt(p, event) == p) this->rightClick(p);
    return;
  }
  if (event.getButton() != agui::MouseButton::LEFT || this->pressPoint != p) return;

  const Point target  = this->pointAt(p, event);
  const bool  wasDrag = this->dragging;
  this->pressPoint = this->dragTarget = {};
  this->dragging   = false;
  this->dirty      = true;

  if (!target.valid()) return;  // let go off the board: nothing
  if (wasDrag && target != p) {
    this->report(this->game->moveStone(p, target));
    return;
  }
  if (target == p) this->click(p, event.control(), event.shift());
}

void BoardView::click(Point p, bool ctrl, bool shift)
{
  const Position& position = this->game->position();
  const std::optional<Mark> mark = ToolMark(this->tool);

  switch (this->tool) {
  case Tool::Play:
    if (shift) {
      this->rightClick(p);
    } else if (ctrl) {
      this->report(this->game->insertMove(p));
    } else if (position.at(p) == Stone::None) {
      this->report(this->game->play(p));
    }
    return;
  case Tool::Black: this->report(this->game->setupStone(p, Stone::Black)); return;
  case Tool::White: this->report(this->game->setupStone(p, Stone::White)); return;
  case Tool::Erase: this->report(this->game->setupStone(p, Stone::None)); return;

  case Tool::Letter:
  case Tool::Number:
  case Tool::Text: {
    // A label already there comes off; otherwise the next one goes on.
    const std::string prefix = ToSgf(p) + ":";
    for (const std::string& lb : this->game->current().values("LB")) {
      if (lb.rfind(prefix, 0) == 0) {
        this->game->toggleLabel(p, lb.substr(prefix.size()));
        return;
      }
    }
    const std::string label = this->tool == Tool::Letter ? this->game->nextLetter()
                            : this->tool == Tool::Number ? this->game->nextNumber()
                                                         : this->text;
    if (label.empty()) {
      this->report(Outcome::Fail("Type the label's text into the box beside the tools first."));
      return;
    }
    this->game->toggleLabel(p, label);
    return;
  }

  case Tool::Arrow:
  case Tool::Line:
    if (!this->lineStart.valid()) {
      this->lineStart = p;
    } else {
      if (p != this->lineStart) this->game->toggleLine(this->lineStart, p, this->tool == Tool::Arrow);
      this->lineStart = {};
    }
    this->dirty = true;
    return;

  default:
    if (mark) this->game->toggleMark(p, *mark);
    return;
  }
}

void BoardView::rightClick(Point p)
{
  if (this->tool == Tool::Play || this->tool == Tool::Black || this->tool == Tool::White) {
    // To the move that played this stone.
    if (this->game->goToStone(p) && Game::MoveColor(this->game->current()) == Stone::None) {
      this->report(Outcome::Done("That stone was set up here, not played."));
    }
    return;
  }

  // With a marking tool, clears whatever marks the point.
  sgf::Node& node = this->game->current();
  const std::string v = ToSgf(p);
  for (Mark m : { Mark::Triangle, Mark::Square, Mark::Circle, Mark::Cross, Mark::Selected, Mark::TerritoryBlack,
                  Mark::TerritoryWhite, Mark::Dim }) {
    const std::vector<Point> marked = PointList(node.values(MarkProperty(m)));
    if (std::find(marked.begin(), marked.end(), p) != marked.end()) this->game->toggleMark(p, m);
  }
  for (const std::string& lb : node.values("LB")) {
    if (lb.rfind(v + ":", 0) == 0) {
      this->game->toggleLabel(p, lb.substr(3));
      break;
    }
  }
}

void BoardView::report(const Outcome& outcome)
{
  this->dirty = true;
  if (this->onOutcome) this->onOutcome(outcome);
}

}  // namespace ui
