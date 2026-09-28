#include <ui/GoSprites.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace ui {

namespace {

// Each picture is a square of this many texels, drawn at whatever size the
// board's points work out to. Big enough to stay smooth on a 4K screen at a
// large interface scale; mipmaps take care of the small end.
constexpr int CELL    = 128;
constexpr int COLUMNS = 8;
constexpr int SHEET_W = CELL * COLUMNS;
constexpr int SHEET_H = CELL * 4;

// What an image reports as its own size -- only a hint, since every picture
// is drawn at the size of the widget holding it.
constexpr float IMAGE_SCALE = 0.25f;

struct Rgba {
  float r, g, b, a;
};

Rgba Rgb(int r, int g, int b, float a = 1.0f)
{
  return { float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, a };
}

Rgba Mix(Rgba a, Rgba b, float t)
{
  return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
}

float Clamp01(float v)
{
  return std::clamp(v, 0.0f, 1.0f);
}

float SmoothStep(float edge0, float edge1, float x)
{
  const float t = Clamp01((x - edge0) / (edge1 - edge0));
  return t * t * (3.0f - 2.0f * t);
}

// --- signed distances, in the picture's own units: -1 to 1 across it ---

float Length(float x, float y)
{
  return std::sqrt(x * x + y * y);
}

float Segment(float px, float py, float ax, float ay, float bx, float by)
{
  const float dx = bx - ax, dy = by - ay;
  const float t  = Clamp01(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy));
  return Length(px - ax - t * dx, py - ay - t * dy);
}

float Box(float px, float py, float half)
{
  const float qx = std::abs(px) - half, qy = std::abs(py) - half;
  return Length(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f);
}

// How much of a texel a shape covers, from its distance: an edge one texel
// wide, which is what anti-aliases it.
constexpr float TEXEL = 2.0f / float(CELL);

float Cover(float distance)
{
  return Clamp01(0.5f - distance / TEXEL);
}

// A picture being painted: layers laid over each other, then copied into its
// cell of the sheet.
class Canvas {
public:
  // `under` is the colour of the fully transparent texels. It matters: the
  // mipmaps average colour with its neighbours whatever their alpha, and
  // black under a white stone's edge would give it a grey rim when small.
  explicit Canvas(Rgba under)
      : pixels(size_t(CELL) * CELL, Rgba{ under.r, under.g, under.b, 0.0f })
  {}

  // Lays `paint` over everything so far. It gets the texel's centre in -1..1
  // and returns a colour whose alpha is how much of it covers the texel.
  void layer(const std::function<Rgba(float x, float y)>& paint)
  {
    for (int j = 0; j < CELL; ++j) {
      for (int i = 0; i < CELL; ++i) {
        const float x   = (float(i) + 0.5f) / float(CELL) * 2.0f - 1.0f;
        const float y   = (float(j) + 0.5f) / float(CELL) * 2.0f - 1.0f;
        const Rgba  src = paint(x, y);
        Rgba&       dst = this->pixels[size_t(j) * CELL + size_t(i)];
        const float a   = src.a + dst.a * (1.0f - src.a);
        if (a <= 0.0f) continue;
        const float keep = dst.a * (1.0f - src.a);
        dst = { (src.r * src.a + dst.r * keep) / a, (src.g * src.a + dst.g * keep) / a,
                (src.b * src.a + dst.b * keep) / a, a };
      }
    }
  }

  void copyTo(::Image& sheet, int index) const
  {
    auto* out = static_cast<::Color*>(sheet.data);
    const int ox = (index % COLUMNS) * CELL;
    const int oy = (index / COLUMNS) * CELL;
    for (int j = 0; j < CELL; ++j) {
      for (int i = 0; i < CELL; ++i) {
        const Rgba& p = this->pixels[size_t(j) * CELL + size_t(i)];
        out[size_t(oy + j) * SHEET_W + size_t(ox + i)] = {
          static_cast<unsigned char>(std::lround(Clamp01(p.r) * 255.0f)),
          static_cast<unsigned char>(std::lround(Clamp01(p.g) * 255.0f)),
          static_cast<unsigned char>(std::lround(Clamp01(p.b) * 255.0f)),
          static_cast<unsigned char>(std::lround(Clamp01(p.a) * 255.0f)),
        };
      }
    }
  }

private:
  std::vector<Rgba> pixels;
};

// A stone fills this much of its point, so neighbours nearly touch the way
// real ones do.
constexpr float STONE_R = 0.93f;

// The soft shadow a stone casts down and to the right.
void Shadow(Canvas& canvas)
{
  canvas.layer([](float x, float y) {
    const float d = Length(x - 0.05f, y - 0.07f) - STONE_R;
    return Rgb(0, 0, 0, 0.35f * (1.0f - SmoothStep(-0.06f, 0.06f, d)));
  });
}

void BlackStone(Canvas& canvas)
{
  Shadow(canvas);
  canvas.layer([](float x, float y) {
    // Slate: nearly black, with a soft sheen up and to the left.
    const float sheen = Length(x + 0.35f, y + 0.42f);
    Rgba c = Mix(Rgb(92, 92, 98), Rgb(16, 16, 18), SmoothStep(0.0f, 0.85f, sheen));
    c.a = Cover(Length(x, y) - STONE_R);
    return c;
  });
}

void WhiteStone(Canvas& canvas)
{
  Shadow(canvas);
  canvas.layer([](float x, float y) {
    // Shell: lit from the top left, a little darker towards its rim.
    const float r     = Length(x, y);
    const float light = Clamp01((x + y) * 0.35f + 0.5f);
    Rgba c = Mix(Rgb(252, 252, 247), Rgb(196, 196, 188), light);
    c = Mix(c, Rgb(150, 150, 142), 0.5f * SmoothStep(0.72f * STONE_R, STONE_R, r));
    c.a = Cover(r - STONE_R);
    return c;
  });
}

// --- marks, drawn as lines THICK wide ---
constexpr float THICK = 0.11f;

using Shape = std::function<float(float x, float y)>;

void Outline(Canvas& canvas, Rgba color, const Shape& distance)
{
  canvas.layer([&](float x, float y) {
    Rgba c = color;
    c.a *= Cover(distance(x, y) - THICK * 0.5f);
    return c;
  });
}

void Filled(Canvas& canvas, Rgba color, const Shape& distance)
{
  canvas.layer([&](float x, float y) {
    Rgba c = color;
    c.a *= Cover(distance(x, y));
    return c;
  });
}

float TriangleEdges(float x, float y)
{
  // An equilateral triangle, point up, sitting a touch low so it looks
  // centred on the point.
  constexpr float R = 0.56f;
  const float ax = 0.0f, ay = -R + 0.06f;
  const float bx = R * 0.866f, by = R * 0.5f + 0.06f;
  const float cx = -R * 0.866f, cy = R * 0.5f + 0.06f;
  return std::min({ Segment(x, y, ax, ay, bx, by), Segment(x, y, bx, by, cx, cy), Segment(x, y, cx, cy, ax, ay) });
}

float SquareEdges(float x, float y)
{
  return std::abs(Box(x, y, 0.38f));
}

float CircleEdge(float x, float y)
{
  return std::abs(Length(x, y) - 0.42f);
}

float CrossLines(float x, float y)
{
  constexpr float H = 0.34f;
  return std::min(Segment(x, y, -H, -H, H, H), Segment(x, y, -H, H, H, -H));
}

void Paint(Sprite sprite, ::Image& sheet)
{
  const Rgba dark  = Rgb(18, 18, 18);
  const Rgba light = Rgb(246, 246, 246);
  const Rgba wire  = Rgb(176, 176, 176);  // the tree's lines, on a dark panel

  switch (sprite) {
  case Sprite::BlackStone: {
    Canvas c(Rgb(16, 16, 18));
    BlackStone(c);
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::WhiteStone: {
    Canvas c(Rgb(230, 230, 225));
    WhiteStone(c);
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TriangleDark:
  case Sprite::TriangleLight:
  case Sprite::SquareDark:
  case Sprite::SquareLight:
  case Sprite::CircleDark:
  case Sprite::CircleLight:
  case Sprite::CrossDark:
  case Sprite::CrossLight: {
    const int  n     = int(sprite) - int(Sprite::TriangleDark);
    const Rgba color = n % 2 == 0 ? dark : light;
    const Shape shapes[] = { TriangleEdges, SquareEdges, CircleEdge, CrossLines };
    Canvas c(color);
    Outline(c, color, shapes[n / 2]);
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::LastMove: {
    const Rgba red = Rgb(232, 64, 44);
    Canvas c(red);
    Filled(c, red, [](float x, float y) { return Length(x, y) - 0.26f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::Selected: {
    const Rgba blue = Rgb(60, 130, 255, 0.45f);
    Canvas c(blue);
    Filled(c, blue, [](float x, float y) { return Box(x, y, 0.86f); });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TerritoryBlack:
  case Sprite::TerritoryWhite: {
    const bool black = sprite == Sprite::TerritoryBlack;
    Canvas c(black ? dark : light);
    // An edge in the other colour, so it shows on a stone of its own colour.
    Filled(c, black ? light : dark, [](float x, float y) { return Box(x, y, 0.36f); });
    Filled(c, black ? dark : light, [](float x, float y) { return Box(x, y, 0.28f); });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::StarPoint: {
    const Rgba ink = Rgb(40, 28, 12);
    Canvas c(ink);
    Filled(c, ink, [](float x, float y) { return Length(x, y) - 0.15f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TreeSetup: {
    const Rgba grey = Rgb(150, 150, 150);
    Canvas c(grey);
    Filled(c, Rgb(40, 40, 40), [](float x, float y) { return Box(x, y, 0.56f) - 0.1f; });
    Filled(c, grey, [](float x, float y) { return Box(x, y, 0.46f) - 0.1f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TreeHorizontal: {
    Canvas c(wire);
    Filled(c, wire, [](float, float y) { return std::abs(y) - 0.1f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TreeVertical: {
    Canvas c(wire);
    Filled(c, wire, [](float x, float) { return std::abs(x) - 0.1f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::TreeDiagonal: {
    Canvas c(wire);
    Filled(c, wire, [](float x, float y) { return Segment(x, y, -1.2f, -1.2f, 1.2f, 1.2f) - 0.1f; });
    c.copyTo(sheet, int(sprite));
    break;
  }
  case Sprite::Count:
    break;
  }
}

}  // namespace

GoSprites::GoSprites()
{
  ::Image pixels = GenImageColor(SHEET_W, SHEET_H, ::Color{ 0, 0, 0, 0 });
  for (int s = 0; s < int(Sprite::Count); ++s) Paint(Sprite(s), pixels);
  this->sheet = agui_raylib::MakeSharedTexture(pixels);
  UnloadImage(pixels);

  if (this->sheet) {
    GenTextureMipmaps(this->sheet.get());
    SetTextureFilter(*this->sheet, TEXTURE_FILTER_TRILINEAR);
  }
}

std::unique_ptr<agui::Image> GoSprites::image(Sprite sprite) const
{
  if (!this->sheet) return nullptr;
  const int index = int(sprite);
  // Half a texel in from the edges, so neighbouring pictures on the sheet
  // don't bleed in when it is sampled.
  const ::Rectangle region{ float((index % COLUMNS) * CELL) + 0.5f, float((index / COLUMNS) * CELL) + 0.5f,
                            float(CELL) - 1.0f, float(CELL) - 1.0f };
  return std::make_unique<agui_raylib::RaylibImage>(this->sheet, region, IMAGE_SCALE);
}

}  // namespace ui
