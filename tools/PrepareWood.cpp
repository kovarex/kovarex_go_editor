// Makes the board's wood from the photograph, at build time rather than every
// start-up: turned so the grain runs down the board, with only how light each
// texel of the photo is kept of it, stretched so its lightest and darkest (bar
// the odd speck) span the whole range.
//
// What it writes is how far each texel is from the board's light wood towards
// its grain -- 0 the one, GRAIN_RANGE the other -- as a grayscale JPEG: the
// colours themselves are put on when it is loaded (ui/GoSprites.cpp), so the
// file has one channel to carry rather than three. The range is about as many
// steps as there are between the light wood's colour and the grain's, so no
// more detail is kept than can show -- and a JPEG of it is a third the size.
//
// Worked out per texel of a 2048 x 2048 photo, that took the editor a third
// of a second to start; now it only has to decode the result.
//   PrepareWood <photo.jpg> <wood.jpg>
// Built and run by fbuild.bff.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// JPEG quality: the grain is soft, and shown no bigger than it is; even
// enlarged, 75 looks the same as 95.
constexpr int QUALITY = 75;
// Must match GoSprites.cpp's.
constexpr float GRAIN_RANGE = 64.0f;

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: PrepareWood <photo.jpg> <wood.jpg>\n");
    return 1;
  }
  int w = 0, h = 0, channels = 0;
  unsigned char* photo = stbi_load(argv[1], &w, &h, &channels, 3);
  if (!photo) {
    std::fprintf(stderr, "PrepareWood: can't read %s: %s\n", argv[1], stbi_failure_reason());
    return 1;
  }

  // How light each texel is, turned a quarter clockwise: the photo's row y
  // becomes the column h - 1 - y, so the new picture is h wide.
  std::vector<float> light(size_t(w) * size_t(h));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const unsigned char* c = photo + (size_t(y) * size_t(w) + size_t(x)) * 3;
      light[size_t(x) * size_t(h) + size_t(h - 1 - y)] = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
    }
  }
  stbi_image_free(photo);

  // The lightest and darkest but for the lightest and darkest 2%.
  std::vector<size_t> histogram(256, 0);
  for (float l : light) ++histogram[size_t(std::clamp(std::lround(l), 0L, 255L))];
  const auto percentile = [&](double fraction) {
    size_t seen = 0;
    for (int level = 0; level < 256; ++level) {
      seen += histogram[size_t(level)];
      if (double(seen) >= fraction * double(light.size())) return float(level);
    }
    return 255.0f;
  };
  const float darkest = percentile(0.02), lightest = percentile(0.98);

  std::vector<unsigned char> grain(light.size());
  for (size_t i = 0; i < light.size(); ++i) {
    // To a power, so the wood between the grain stays light and only the
    // grain itself darkens.
    const float t = std::pow(std::clamp((lightest - light[i]) / std::max(1.0f, lightest - darkest), 0.0f, 1.0f), 1.6f);
    grain[i] = static_cast<unsigned char>(std::lround(t * GRAIN_RANGE));
  }

  // Width and height swap with the turn.
  if (!stbi_write_jpg(argv[2], h, w, 1, grain.data(), QUALITY)) {
    std::fprintf(stderr, "PrepareWood: can't write %s\n", argv[2]);
    return 1;
  }
  return 0;
}
