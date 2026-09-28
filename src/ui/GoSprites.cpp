#include <ui/GoSprites.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

// --- noise, for what makes real stones each a little different ---

// A number in -1..1 that looks random but is always the same for `n`.
float Hash(int seed)
{
  uint32_t n = uint32_t(seed);
  n = (n << 13) ^ n;
  n = n * (n * n * 15731u + 789221u) + 1376312589u;
  return 1.0f - float(n & 0x7fffffffu) / 1073741824.0f;
}

// Smooth noise along a line: -1..1, changing about once per unit.
float Noise(float x, int seed)
{
  const float fl = std::floor(x);
  const int   i  = int(fl);
  const float f  = x - fl;
  const float t  = f * f * (3.0f - 2.0f * f);
  return Hash(i + seed * 7919) + (Hash(i + 1 + seed * 7919) - Hash(i + seed * 7919)) * t;
}

// The same over the plane.
float Noise(float x, float y, int seed)
{
  const float fy = std::floor(y);
  const int   j  = int(fy);
  const float t  = (y - fy) * (y - fy) * (3.0f - 2.0f * (y - fy));
  const float a  = Noise(x, seed + j * 131), b = Noise(x, seed + (j + 1) * 131);
  return a + (b - a) * t;
}

// A stone is a lens, domed on top: the light falls on it from the top left,
// a little in front. How brightly a point is lit, and how near it is to the
// mirror image of the light -- the highlight.
struct Lit {
  float diffuse;   // 0..1
  float specular;  // 0..1
};

Lit Light(float x, float y)
{
  const float u  = x / STONE_R, v = y / STONE_R;
  const float r2 = std::min(1.0f, u * u + v * v);
  // The dome's height, flattened: a go stone is far from a ball.
  const float h = 0.55f * std::sqrt(1.0f - r2);
  float nx = u, ny = v, nz = h + 0.35f;
  const float n = std::sqrt(nx * nx + ny * ny + nz * nz);
  nx /= n, ny /= n, nz /= n;
  constexpr float LX = -0.45f, LY = -0.55f, LZ = 0.70f;  // toward the light, normalised
  const float diffuse = Clamp01(nx * LX + ny * LY + nz * LZ);
  // Blinn: halfway between the light and the eye (straight above).
  constexpr float HX = -0.26f, HY = -0.32f, HZ = 0.91f;
  const float specular = std::pow(Clamp01(nx * HX + ny * HY + nz * HZ), 40.0f);
  return { diffuse, specular };
}

// The shadow a stone casts: a soft dark disc, in a picture SHADOW_SCALE
// times the stone's so it has room to fade out. The board puts it under the
// stone, shifted down and to the right.
void Shadow(Canvas& canvas)
{
  canvas.layer([](float x, float y) {
    const float d = Length(x, y) - STONE_R / SHADOW_SCALE;
    return Rgb(0, 0, 0, 0.55f * (1.0f - SmoothStep(-0.14f, 0.16f, d)));
  });
}

void BlackStone(Canvas& canvas)
{
  canvas.layer([](float x, float y) {
    // Slate: a deep, cool black, matte, with a broad soft sheen where the
    // light falls, a small brighter glint, and the stone's fine grain.
    const Lit   lit   = Light(x, y);
    const float grain = 0.5f * Noise(x * 40.0f, y * 40.0f, 3) + 0.25f * Noise(x * 90.0f, y * 90.0f, 5);
    Rgba c = Mix(Rgb(6, 6, 8), Rgb(58, 60, 66), std::pow(lit.diffuse, 3.0f));
    c = Mix(c, Rgb(150, 154, 162), 0.55f * lit.specular);
    c.r += grain * 0.012f, c.g += grain * 0.012f, c.b += grain * 0.014f;
    c.a = Cover(Length(x, y) - STONE_R);
    return c;
  });
}

// How many different white stones there are: clamshells, each with growth
// lines of its own.
constexpr int SHELLS = int(Sprite::WhiteStone8) - int(Sprite::WhiteStone) + 1;

void WhiteStone(Canvas& canvas, int shell)
{

  // The shell the stone was cut from grew in rings round its hinge, well off
  // the stone: so the lines run across it in gentle arcs, one way on this
  // stone and another on the next, closer here and wider there.
  const float angle   = 6.2832f * (float(shell) + 0.5f * Hash(shell * 17 + 1)) / float(SHELLS);
  const float hinge   = 1.9f + 0.6f * Hash(shell * 17 + 2);
  const float hx      = hinge * std::cos(angle), hy = hinge * std::sin(angle);
  const float lines   = 6.5f + 1.0f * Hash(shell * 17 + 3);  // lines per unit of radius
  const float contrast = 0.8f + 0.2f * Hash(shell * 17 + 4);
  const int   seed    = 100 + shell * 13;

  canvas.layer([=](float x, float y) {
    const float d = Length(x - hx, y - hy);
    // Growth that sped up and slowed down: the lines' spacing wanders.
    const float ring = d * lines + 1.6f * Noise(d * 2.0f, seed) + 0.5f * Noise(d * 7.0f, seed + 1);
    // Grey growth lines, some stronger than others; finer ones between them;
    // and a faint broader banding under it all.
    const float line  = std::pow(0.5f + 0.5f * std::cos(6.2832f * ring), 3.0f);
    const float fine  = std::pow(0.5f + 0.5f * std::cos(6.2832f * ring * 2.7f + 1.3f), 6.0f);
    const float bands = 0.5f + 0.5f * Noise(ring * 0.4f, seed + 2);
    const float grain = contrast * (0.75f * line * (0.3f + 0.7f * bands) + 0.25f * fine + 0.15f * bands);

    Rgba shell = Mix(Rgb(250, 249, 244), Rgb(196, 194, 184), Clamp01(grain));
    const Lit lit = Light(x, y);
    // Lit, and shaded a little toward grey away from the light.
    Rgba c = Mix(Mix(shell, Rgb(160, 160, 156), 0.4f), shell, std::pow(lit.diffuse, 0.7f));
    c = Mix(c, Rgb(255, 255, 255), 0.7f * lit.specular);
    // A darker rim, which a real stone's bevel has, and which keeps it apart
    // from a neighbour and the wood.
    c = Mix(c, Rgb(128, 128, 122), 0.45f * SmoothStep(0.86f * STONE_R, STONE_R, Length(x, y)));
    c.a = Cover(Length(x, y) - STONE_R);
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
  case Sprite::WhiteStone:
  case Sprite::WhiteStone2:
  case Sprite::WhiteStone3:
  case Sprite::WhiteStone4:
  case Sprite::WhiteStone5:
  case Sprite::WhiteStone6:
  case Sprite::WhiteStone7:
  case Sprite::WhiteStone8: {
    Canvas c(Rgb(230, 230, 225));
    WhiteStone(c, int(sprite) - int(Sprite::WhiteStone));
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
  case Sprite::StoneShadow: {
    Canvas c(Rgb(0, 0, 0));
    Shadow(c);
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

// The wood: a photograph of flat-sawn wood, its grain sweeping in the long
// arches of a board cut along the log (resources/wood/LICENSE.txt). Embedded
// by tools/Embed.cpp at build time.
extern const unsigned char BOARD_WOOD_JPG[];
extern const std::size_t   BOARD_WOOD_JPG_SIZE;

namespace {

// The photograph as the board wants it: turned so the grain runs down the
// board, and in the board's colours -- its light wood a warm honey, its
// grain a mid brown. Only how light each texel is, is kept of the photo's
// own colour, stretched so its lightest and darkest (bar the odd speck)
// span the two.
::Image WoodGrain()
{
  ::Image image = LoadImageFromMemory(".jpg", BOARD_WOOD_JPG, int(BOARD_WOOD_JPG_SIZE));
  if (!image.data) return GenImageColor(16, 16, ::Color{ 220, 179, 105, 255 });
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  ImageRotateCW(&image);

  auto*        pixels = static_cast<::Color*>(image.data);
  const size_t count  = size_t(image.width) * size_t(image.height);
  const auto   light  = [](const ::Color& c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; };

  std::vector<int> histogram(256, 0);
  for (size_t i = 0; i < count; ++i) ++histogram[size_t(std::lround(light(pixels[i])))];
  const auto percentile = [&](float fraction) {
    size_t seen = 0;
    for (int level = 0; level < 256; ++level) {
      seen += size_t(histogram[size_t(level)]);
      if (float(seen) >= fraction * float(count)) return float(level);
    }
    return 255.0f;
  };
  const float darkest = percentile(0.02f), lightest = percentile(0.98f);

  const Rgba honey = Rgb(232, 198, 132);
  const Rgba brown = Rgb(192, 146, 82);
  for (size_t i = 0; i < count; ++i) {
    // Squared, so the wood between the grain stays light and only the grain
    // itself darkens.
    const float t = std::pow(Clamp01((lightest - light(pixels[i])) / std::max(1.0f, lightest - darkest)), 1.6f);
    const Rgba  c = Mix(honey, brown, t);
    pixels[i]     = { static_cast<unsigned char>(std::lround(Clamp01(c.r) * 255.0f)),
                      static_cast<unsigned char>(std::lround(Clamp01(c.g) * 255.0f)),
                      static_cast<unsigned char>(std::lround(Clamp01(c.b) * 255.0f)), 255 };
  }
  return image;
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

  ::Image wood = WoodGrain();
  this->woodTexture = agui_raylib::MakeSharedTexture(wood);
  UnloadImage(wood);
  if (this->woodTexture) {
    GenTextureMipmaps(this->woodTexture.get());
    SetTextureFilter(*this->woodTexture, TEXTURE_FILTER_TRILINEAR);
  }
}

std::unique_ptr<agui::Image> GoSprites::wood(float u0, float v0, float u1, float v1) const
{
  if (!this->woodTexture) return nullptr;
  const float w = float(this->woodTexture->width), h = float(this->woodTexture->height);
  const ::Rectangle region{ u0 * w, v0 * h, (u1 - u0) * w, (v1 - v0) * h };
  return std::make_unique<agui_raylib::RaylibImage>(this->woodTexture, region, 1.0f);
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
