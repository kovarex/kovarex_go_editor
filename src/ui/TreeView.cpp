#include <ui/TreeView.hpp>

#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/ImageWidget.hpp>

#include <algorithm>

namespace ui {

namespace {

// From one node to the next, across and down.
constexpr int PITCH = Theme::TREE_NODE_PX + 6;
// Round the whole tree, so the first node isn't against the pane's edge.
constexpr int PAD = 4;
// The stone on a node, inside its button.
constexpr int ICON = Theme::TREE_NODE_PX - 6;

void Pin(agui::Widget& widget, agui::Style& style, int w, int h)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
  style.setMinimalHeight(h);
  style.setMaximalHeight(h);
  widget.setSize(w, h, agui::SetSizeInfo());
}

// Puts a node's stone in the middle of its button. Children sit inside the
// button's padding, which includes the border of whatever it is drawn with --
// and the current node's highlight has one where the others have none -- so
// this has to be done again whenever the button's style changes.
void CentreIcon(agui::Button& button)
{
  if (button.getChildCount() == 0) return;
  const int at = (Theme::TREE_NODE_PX - ICON) / 2;
  button.getChildAt(0u)->setLocation(at - button.getLeftPadding(), at - button.getTopPadding());
}

// The middle of a grid cell of the tree.
int Centre(int cell)
{
  return PAD + cell * PITCH + PITCH / 2;
}

}  // namespace

TreeView::TreeView(Theme& theme, const GoSprites& sprites)
    : theme(theme)
    , sprites(sprites)
{
  this->pane.setHScrollPolicy(agui::ScrollPolicy::Auto);
  this->pane.setVScrollPolicy(agui::ScrollPolicy::Auto);
  this->canvas = &make<agui::EmptyWidget>();
  this->pane.add(this->canvas);
}

void TreeView::show(Game* newGame)
{
  this->game      = newGame;
  this->shownTree = 0;
  this->refresh();
}

void TreeView::setSize(int width, int height)
{
  if (width == this->pane.getWidth() && height == this->pane.getHeight()) return;
  Pin(this->pane, this->pane.style, width, height);
}

void TreeView::refresh()
{
  if (!this->game) {
    if (this->shownTree != 0) {
      this->canvas->clear();
      this->buttons.clear();
      this->lit       = nullptr;
      this->shownTree = 0;
    }
    return;
  }

  if (this->shownTree != this->game->treeRevision()) {
    this->rebuild();
    this->shownTree     = this->game->treeRevision();
    this->shownRevision = 0;
  }

  if (this->shownRevision != this->game->revision()) {
    this->shownRevision = this->game->revision();
    const auto    it  = this->buttons.find(&this->game->current());
    agui::Button* now = it == this->buttons.end() ? nullptr : it->second;
    if (now != this->lit) {
      if (this->lit) {
        this->lit->style.setParent(&this->theme.treeNode);
        CentreIcon(*this->lit);
      }
      if (now) {
        now->style.setParent(&this->theme.treeNodeCurrent);
        CentreIcon(*now);
      }
      this->lit = now;
    }
    if (now) this->pane.scrollToMakeWidgetVisible(now, ScrollMode::InView);
  }
}

void TreeView::placeLine(sgf::Node* start, int column, int minRow)
{
  std::vector<sgf::Node*> line;
  for (sgf::Node* n = start;; n = &n->child(0)) {
    line.push_back(n);
    if (n->childCount() == 0) break;
  }

  // The row has to be free from the parent's column -- the branch's line
  // drops down that one -- to the end of this line.
  const int from = start->parent() ? column - 1 : column;
  const int to   = column + int(line.size()) - 1;
  if (int(this->nextFree.size()) <= to) this->nextFree.resize(size_t(to) + 1, 0);
  int row = minRow;
  for (int k = from; k <= to; ++k) row = std::max(row, this->nextFree[size_t(k)]);
  for (int k = from; k <= to; ++k) this->nextFree[size_t(k)] = row + 1;

  for (size_t i = 0; i < line.size(); ++i) this->placed.push_back({ line[i], column + int(i), row });

  // The variations, from the far end of the line back. A branch nearer the
  // end is placed first and takes the rows nearest the line; one from
  // further back drops past it and runs underneath, so no two lines cross.
  for (size_t i = line.size(); i-- > 0;) {
    for (size_t k = 1; k < line[i]->childCount(); ++k) this->placeLine(&line[i]->child(k), column + int(i) + 1, row + 1);
  }
}

void TreeView::rebuild()
{
  this->canvas->clear();
  this->buttons.clear();
  this->lit = nullptr;
  this->placed.clear();
  this->nextFree.clear();

  this->placeLine(&this->game->root(), 0, 0);

  std::unordered_map<const sgf::Node*, std::pair<int, int>> where;
  int columns = 1, rows = 1;
  for (const Placed& p : this->placed) {
    where[p.node] = { p.column, p.row };
    columns = std::max(columns, p.column + 1);
    rows    = std::max(rows, p.row + 1);
  }

  // The lines first, so the nodes sit on top of them.
  const auto picture = [this](Sprite sprite, int x, int y, int w, int h) {
    agui::ImageWidget& image = make<agui::ImageWidget>(this->sprites.image(sprite), true);
    image.setIgnoredByInteraction(true);
    Pin(image, image.style, w, h);
    image.setLocation(x, y);
    *this->canvas << image;
  };
  for (const Placed& p : this->placed) {
    // On along the line to the next move.
    if (p.node->childCount() > 0) picture(Sprite::TreeHorizontal, Centre(p.column), PAD + p.row * PITCH, PITCH, PITCH);

    // Down from the move a variation branches off, then across to it.
    if (p.node->parent() && p.node->indexInParent() > 0) {
      const auto [parentColumn, parentRow] = where[p.node->parent()];
      if (p.row - 1 > parentRow) {
        picture(Sprite::TreeVertical, PAD + parentColumn * PITCH, Centre(parentRow), PITCH, (p.row - 1 - parentRow) * PITCH);
      }
      picture(Sprite::TreeDiagonal, Centre(parentColumn), Centre(p.row - 1), PITCH, PITCH);
    }
  }

  for (const Placed& p : this->placed) {
    agui::Button& button = make<agui::Button>(&this->theme.treeNode);
    button.setFocusable(false);

    const Stone color = Game::MoveColor(*p.node);
    agui::ImageWidget& icon = make<agui::ImageWidget>(
        this->sprites.image(color == Stone::Black ? Sprite::BlackStone : color == Stone::White ? Sprite::WhiteStone : Sprite::TreeSetup));
    icon.scaleToKeepTheRatio = true;
    icon.setIgnoredByInteraction(true);
    if (this->game->isPass(*p.node)) icon.opacity = 0.4;  // a pass: a move, but no stone
    button << icon;
    Pin(icon, icon.style, ICON, ICON);
    CentreIcon(button);

    // A comment shows when the node is hovered, so commented moves can be found.
    if (p.node->has("C")) button.setToolTip(p.node->get("C").substr(0, 300));

    sgf::Node* node = p.node;
    button.onClick(this, [this, node] { this->game->goTo(*node); });
    const int offset = (PITCH - Theme::TREE_NODE_PX) / 2;
    button.setLocation(PAD + p.column * PITCH + offset, PAD + p.row * PITCH + offset);
    *this->canvas << button;
    this->buttons[p.node] = &button;
  }

  Pin(*this->canvas, this->canvas->style, columns * PITCH + 2 * PAD, rows * PITCH + 2 * PAD);
}

}  // namespace ui
