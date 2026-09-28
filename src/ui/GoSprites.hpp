// The board's pictures: stones, the SGF marks, the last-move dot, star
// points and the game tree's connecting lines. They are painted once at
// start-up into one texture -- shaded stones, anti-aliased marks -- rather
// than shipped as image files, so they come out at a resolution that stays
// clean from a small window to a 4K one, with mipmaps for the small end.
//
// They are handed out as Agui images, because that is what draws them: a
// point on the board is a widget, and a stone on it is an ImageWidget like
// any other.

#pragma once

#include <ui/AguiRaylib.hpp>

#include <memory>

namespace agui {
class Image;
}

namespace ui {

enum class Sprite {
  BlackStone,
  WhiteStone,
  // Marks come in a dark version, for empty points and white stones, and a
  // light one for black stones.
  TriangleDark,
  TriangleLight,
  SquareDark,
  SquareLight,
  CircleDark,
  CircleLight,
  CrossDark,
  CrossLight,
  LastMove,        // the dot on the stone just played
  Selected,        // SL: a tinted square over the point
  TerritoryBlack,  // TB and TW: small squares
  TerritoryWhite,
  StarPoint,
  // The game tree: a node that is not a move, and the lines between nodes.
  TreeSetup,
  TreeHorizontal,
  TreeVertical,
  TreeDiagonal,
  // Buttons: reset to defaults, a circling arrow.
  Reset,
  Count
};

class GoSprites {
public:
  // Needs the window.
  GoSprites();

  // A fresh image each call: an ImageWidget owns the one it is given.
  std::unique_ptr<agui::Image> image(Sprite sprite) const;

private:
  std::shared_ptr<Texture2D> sheet;
};

}  // namespace ui
