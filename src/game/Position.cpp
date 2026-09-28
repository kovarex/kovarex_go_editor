#include <game/Position.hpp>

#include <algorithm>

namespace {

constexpr int DX[4] = { 1, -1, 0, 0 };
constexpr int DY[4] = { 0, 0, 1, -1 };

int CoordinateValue(char c)
{
  if (c >= 'a' && c <= 'z') return c - 'a';
  if (c >= 'A' && c <= 'Z') return c - 'A' + 26;
  return -1;
}

char CoordinateLetter(int v)
{
  return v < 26 ? char('a' + v) : char('A' + v - 26);
}

// How far in from the edge the star points and handicap stones sit: the
// fourth line on a big board, the third on a small one.
int StarLine(int size)
{
  return size >= 13 ? 3 : 2;
}

}  // namespace

// ---------------------------------------------------------------- points

std::string ToSgf(Point p)
{
  return { CoordinateLetter(p.x), CoordinateLetter(p.y) };
}

std::optional<Point> FromSgf(std::string_view text)
{
  if (text.size() != 2) return std::nullopt;
  const int x = CoordinateValue(text[0]);
  const int y = CoordinateValue(text[1]);
  if (x < 0 || y < 0) return std::nullopt;
  return Point{ x, y };
}

bool IsPass(std::string_view value, int width, int height)
{
  return value.empty() || (value == "tt" && width <= 19 && height <= 19);
}

std::vector<Point> PointList(const std::vector<std::string>& values)
{
  std::vector<Point> points;
  for (const std::string& v : values) {
    if (v.size() == 5 && v[2] == ':') {
      const std::optional<Point> a = FromSgf(std::string_view(v).substr(0, 2));
      const std::optional<Point> b = FromSgf(std::string_view(v).substr(3, 2));
      if (!a || !b) continue;
      for (int y = std::min(a->y, b->y); y <= std::max(a->y, b->y); ++y)
        for (int x = std::min(a->x, b->x); x <= std::max(a->x, b->x); ++x) points.push_back({ x, y });
    } else if (const std::optional<Point> p = FromSgf(v)) {
      points.push_back(*p);
    }
  }
  return points;
}

std::string ColumnName(int x, int width)
{
  // A-T without I, the way Go boards have always been lettered. Past Z the
  // letters run out, so wide boards are numbered across instead.
  if (width > 25) return std::to_string(x + 1);
  const char letter = char('A' + x + (x >= 8 ? 1 : 0));
  return std::string(1, letter);
}

std::string RowName(int y, int height)
{
  return std::to_string(height - y);
}

std::string DisplayName(Point p, int width, int height)
{
  if (width > 25) return ColumnName(p.x, width) + "-" + RowName(p.y, height);
  return ColumnName(p.x, width) + RowName(p.y, height);
}

std::vector<Point> StarPoints(int width, int height)
{
  if (width < 7 || height < 7) return {};

  const auto lines = [](int size) {
    const int e = StarLine(size);
    std::vector<int> at{ e, size - 1 - e };
    // The middle line gets them too where there is one, but only a big board
    // has room for all three on a side; 9 and 13 have just the one in the
    // centre.
    if (size % 2 == 1) at.push_back(size / 2);
    return at;
  };
  const std::vector<int> xs = lines(width);
  const std::vector<int> ys = lines(height);

  std::vector<Point> points;
  for (int x : xs) {
    for (int y : ys) {
      const bool centreX = width % 2 == 1 && x == width / 2;
      const bool centreY = height % 2 == 1 && y == height / 2;
      const bool side    = centreX != centreY;
      if (side && (width < 15 || height < 15)) continue;
      points.push_back({ x, y });
    }
  }
  return points;
}

std::vector<Point> HandicapPoints(int count, int width, int height)
{
  if (count < 2 || count > 9 || width < 7 || height < 7) return {};
  const bool middles = width % 2 == 1 && height % 2 == 1 && width >= 9 && height >= 9;
  if (count > 4 && !middles) return {};

  const int ex = StarLine(width), ey = StarLine(height);
  const int left = ex, right = width - 1 - ex, top = ey, bottom = height - 1 - ey;
  const int cx = width / 2, cy = height / 2;

  // The traditional order: the corners from top right round, then the
  // centre when the count is odd, then the sides.
  std::vector<Point> points{ { right, top }, { left, bottom } };
  if (count >= 3) points.push_back({ right, bottom });
  if (count >= 4) points.push_back({ left, top });
  if (count >= 6) {
    points.push_back({ left, cy });
    points.push_back({ right, cy });
  }
  if (count >= 8) {
    points.push_back({ cx, top });
    points.push_back({ cx, bottom });
  }
  if (count % 2 == 1 && count >= 5) points.push_back({ cx, cy });
  return points;
}

// ---------------------------------------------------------------- position

Position::Position(int width, int height)
    : w(std::clamp(width, 1, MAX_BOARD))
    , h(std::clamp(height, 1, MAX_BOARD))
    , stones(size_t(this->w) * size_t(this->h), Stone::None)
    , origins(this->stones.size(), nullptr)
    , numbers(this->stones.size(), 0)
{}

void Position::set(Point p, Stone s, const sgf::Node* origin)
{
  if (!this->inside(p)) return;
  const size_t i   = this->index(p);
  this->stones[i]  = s;
  this->origins[i] = s == Stone::None ? nullptr : origin;
  this->numbers[i] = 0;
  this->ko = {};
}

bool Position::groupHasLiberty(Point p, std::vector<Point>& group) const
{
  group.clear();
  const Stone color = this->at(p);
  std::vector<bool> seen(this->stones.size(), false);
  std::vector<Point> open{ p };
  seen[this->index(p)] = true;
  bool liberty = false;

  while (!open.empty()) {
    const Point q = open.back();
    open.pop_back();
    group.push_back(q);
    for (int d = 0; d < 4; ++d) {
      const Point n{ q.x + DX[d], q.y + DY[d] };
      if (!this->inside(n)) continue;
      const size_t i = this->index(n);
      if (this->stones[i] == Stone::None) liberty = true;
      else if (this->stones[i] == color && !seen[i]) {
        seen[i] = true;
        open.push_back(n);
      }
    }
  }
  return liberty;
}

int Position::removeGroup(const std::vector<Point>& group)
{
  for (Point q : group) {
    const size_t i   = this->index(q);
    this->stones[i]  = Stone::None;
    this->origins[i] = nullptr;
    this->numbers[i] = 0;
  }
  return int(group.size());
}

int Position::play(Point p, Stone color, const sgf::Node* origin, int moveNumber)
{
  const size_t i   = this->index(p);
  this->stones[i]  = color;
  this->origins[i] = origin;
  this->numbers[i] = moveNumber;

  int taken = 0;
  Point lastTaken;
  std::vector<Point> group;
  for (int d = 0; d < 4; ++d) {
    const Point n{ p.x + DX[d], p.y + DY[d] };
    if (!this->inside(n) || this->at(n) != Opponent(color)) continue;
    if (!this->groupHasLiberty(n, group)) {
      taken += this->removeGroup(group);
      lastTaken = n;
    }
  }
  (color == Stone::Black ? this->blackCaptures : this->whiteCaptures) += taken;

  // Suicide: the move's own group comes off, and counts for the other side.
  if (!this->groupHasLiberty(p, group)) {
    (color == Stone::Black ? this->whiteCaptures : this->blackCaptures) += this->removeGroup(group);
    this->ko = {};
    return taken;
  }

  // A ko: one stone took one stone, and now stands alone with that point as
  // its only liberty -- so taking it straight back would repeat the position.
  this->ko = {};
  if (taken == 1 && group.size() == 1) {
    int liberties = 0;
    for (int d = 0; d < 4; ++d) {
      const Point n{ p.x + DX[d], p.y + DY[d] };
      if (this->inside(n) && this->at(n) == Stone::None) ++liberties;
    }
    if (liberties == 1) this->ko = lastTaken;
  }
  return taken;
}

bool Position::isSuicide(Point p, Stone color) const
{
  // A move that captures always keeps its stone -- the captured stones leave
  // it a liberty -- so the stone being gone afterwards is exactly suicide.
  Position trial = *this;
  trial.play(p, color);
  return trial.at(p) == Stone::None;
}
