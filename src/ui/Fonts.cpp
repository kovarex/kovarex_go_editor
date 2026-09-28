#include <ui/Fonts.hpp>

#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>

namespace ui {

// The .ttf files, embedded by tools/Embed.cpp at build time.
extern const unsigned char FONT_REGULAR_TTF[];
extern const std::size_t   FONT_REGULAR_TTF_SIZE;
extern const unsigned char FONT_SEMIBOLD_TTF[];
extern const std::size_t   FONT_SEMIBOLD_TTF_SIZE;
extern const unsigned char FONT_BOLD_TTF[];
extern const std::size_t   FONT_BOLD_TTF_SIZE;

namespace {

// Titillium Web's hhea ascender + descender (1133 + 388) over its em (1000).
constexpr float LINE_PER_EM = 1.521f;

// Keyed by px * 4 + weight.
std::unordered_map<int, ::Font>& Cache()
{
  static std::unordered_map<int, ::Font> fonts;
  return fonts;
}

}  // namespace

int LineHeight(int size)
{
  return int(std::lround(float(size) * LINE_PER_EM));
}

const ::Font& FontAt(int px, Weight weight)
{
  auto& fonts = Cache();
  const int key = px * 4 + int(weight);
  if (auto it = fonts.find(key); it != fonts.end()) return it->second;

  const auto [data, size] = weight == Weight::Bold       ? std::pair{ FONT_BOLD_TTF, FONT_BOLD_TTF_SIZE }
                          : weight == Weight::SemiBold ? std::pair{ FONT_SEMIBOLD_TTF, FONT_SEMIBOLD_TTF_SIZE }
                                                       : std::pair{ FONT_REGULAR_TTF, FONT_REGULAR_TTF_SIZE };
  // The 95 printable ASCII glyphs.
  ::Font font = LoadFontFromMemory(".ttf", data, int(size), px, nullptr, 0);
  if (font.texture.id == 0) font = GetFontDefault();
  else SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  return fonts.emplace(key, font).first->second;
}

float FontSpacing(int)
{
  return 0.0f;
}

void Text(const char* text, int x, int y, int px, ::Color color)
{
  DrawTextEx(FontAt(px), text, { float(x), float(y) }, float(px), FontSpacing(px), color);
}

int TextWidth(const char* text, int px)
{
  return int(std::ceil(MeasureTextEx(FontAt(px), text, float(px), FontSpacing(px)).x));
}

void UnloadFonts()
{
  const ::Font fallback = GetFontDefault();
  for (auto& [key, font] : Cache())
    if (font.texture.id != fallback.texture.id) UnloadFont(font);
  Cache().clear();
}

}  // namespace ui
