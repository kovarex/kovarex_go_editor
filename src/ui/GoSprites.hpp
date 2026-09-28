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
  // Clamshell white stones, each with growth lines of its own; WhiteShell()
  // picks one.
  WhiteStone,
  WhiteStone2,
  WhiteStone3,
  WhiteStone4,
  WhiteStone5,
  WhiteStone6,
  WhiteStone7,
  WhiteStone8,
  // What a stone casts on the board: a soft dark disc, drawn SHADOW_SCALE
  // times the stone's size so it has room to fade out.
  StoneShadow,
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
  Selected,        // SL: a tinted square over the point
  TerritoryBlack,  // TB and TW: small squares
  TerritoryWhite,
  StarPoint,
  // The game tree: a node that is not a move, and the lines between nodes.
  TreeSetup,
  TreeHorizontal,
  TreeVertical,
  TreeDiagonal,
  // The side panel's Board buttons: turning and mirroring the whole game.
  RotateLeft,
  RotateRight,
  FlipHorizontal,
  FlipVertical,
  Count
};

// How much bigger than its stone the StoneShadow picture is drawn.
constexpr float SHADOW_SCALE = 1.4f;

// One of the white stones, always the same one for the same `seed`: a
// point's stone keeps its looks move after move.
inline Sprite WhiteShell(unsigned seed)
{
  constexpr unsigned SHELLS = unsigned(Sprite::WhiteStone8) - unsigned(Sprite::WhiteStone) + 1;
  seed = (seed ^ (seed >> 16)) * 0x45d9f3bu;
  seed ^= seed >> 16;
  return Sprite(unsigned(Sprite::WhiteStone) + seed % SHELLS);
}

class GoSprites {
public:
  // Needs the window.
  GoSprites();

  // A fresh image each call: an ImageWidget owns the one it is given.
  std::unique_ptr<agui::Image> image(Sprite sprite) const;

  // The board's wood: a photograph of flat-sawn wood, graded to the board's
  // colours, its grain running down the board. One picture, stretched over
  // the whole board; or the part of it from (u0, v0) to
  // (u1, v1), in fractions of it, for whatever has to show the wood under
  // it again.
  std::unique_ptr<agui::Image> wood(float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) const;

private:
  std::shared_ptr<Texture2D> sheet;
  std::shared_ptr<Texture2D> woodTexture;
};

}  // namespace ui
