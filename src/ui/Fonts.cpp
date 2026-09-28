#include <ui/Fonts.hpp>

#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace ui {

namespace {

// In the system font folder, wherever Windows is installed.
std::string SystemFont(const char* file)
{
  const char* windows = std::getenv("WINDIR");
  return std::string(windows ? windows : "C:/Windows") + "/Fonts/" + file;
}

const std::string FONT_PATH      = SystemFont("consola.ttf");
const std::string BOLD_FONT_PATH = SystemFont("consolab.ttf");

// Keyed by px * 2 + bold.
std::unordered_map<int, ::Font>& Cache()
{
  static std::unordered_map<int, ::Font> fonts;
  return fonts;
}

bool HaveTtf()
{
  static const bool have = FileExists(FONT_PATH.c_str());
  return have;
}

bool HaveBoldTtf()
{
  static const bool have = FileExists(BOLD_FONT_PATH.c_str());
  return have;
}

}  // namespace

const ::Font& FontAt(int px, bool bold)
{
  bold = bold && HaveBoldTtf();
  auto& fonts = Cache();
  const int key = px * 2 + (bold ? 1 : 0);
  if (auto it = fonts.find(key); it != fonts.end()) return it->second;

  ::Font font = GetFontDefault();
  if (HaveTtf()) {
    // The 95 printable ASCII glyphs.
    font = LoadFontEx((bold ? BOLD_FONT_PATH : FONT_PATH).c_str(), px, nullptr, 0);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  }
  return fonts.emplace(key, font).first->second;
}

float FontSpacing(int px)
{
  return HaveTtf() ? 0.0f : float(px) / 10.0f;
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
  if (HaveTtf())
    for (auto& [px, font] : Cache()) UnloadFont(font);
  Cache().clear();
}

}  // namespace ui
