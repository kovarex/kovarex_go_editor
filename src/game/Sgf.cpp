#include <game/Sgf.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace sgf {

namespace {

const std::string              NO_VALUE;
const std::vector<std::string> NO_VALUES;

bool IsSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

bool IsUpper(char c)
{
  return c >= 'A' && c <= 'Z';
}

bool IsLower(char c)
{
  return c >= 'a' && c <= 'z';
}

// The inside of a [value], escapes and all, as it is meant to read. A
// backslash takes the next character literally, except a line break, which
// it removes: that is SGF's "soft" line break, put in by programs that wrap
// long lines. Line breaks come out as plain \n whatever the file used.
std::string Unescape(std::string_view raw)
{
  std::string out;
  out.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    char c = raw[i];
    if (c == '\\' && i + 1 < raw.size()) {
      c = raw[++i];
      if (c == '\n' || c == '\r') {
        // \r\n and \n\r are one line break, not two.
        if (i + 1 < raw.size() && (raw[i + 1] == '\n' || raw[i + 1] == '\r') && raw[i + 1] != c) ++i;
        continue;
      }
      out += c;
      continue;
    }
    if (c == '\r') {
      if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
      out += '\n';
      continue;
    }
    out += c;
  }
  return out;
}

void AppendEscaped(std::string& out, std::string_view value)
{
  for (char c : value) {
    if (c == ']' || c == '\\') out += '\\';
    out += c;
  }
}

// A node's properties as ;B[dd]C[...]. The root's go one to a line, since
// there are a lot of them and people read the header; everywhere else they
// run on.
void AppendNode(std::string& out, const Node& node, bool root)
{
  out += ';';
  for (size_t i = 0; i < node.properties().size(); ++i) {
    const Property& p = node.properties()[i];
    if (root && i > 0) out += '\n';
    out += p.id;
    if (p.values.empty()) out += "[]";
    for (const std::string& v : p.values) {
      out += '[';
      AppendEscaped(out, v);
      out += ']';
    }
  }
}

// Line breaks in the output, so a long game is not one line thousands of
// characters long. Only ever between nodes, where SGF ignores whitespace.
void BreakIfLong(std::string& out, size_t& lineStart)
{
  if (out.size() - lineStart > 72) {
    out += '\n';
    lineStart = out.size();
  }
}

}  // namespace

// ---------------------------------------------------------------- node

const Property* Node::find(std::string_view id) const
{
  for (const Property& p : this->props) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

Property* Node::find(std::string_view id)
{
  for (Property& p : this->props) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const std::string& Node::get(std::string_view id) const
{
  const Property* p = this->find(id);
  return p && !p->values.empty() ? p->values.front() : NO_VALUE;
}

const std::vector<std::string>& Node::values(std::string_view id) const
{
  const Property* p = this->find(id);
  return p ? p->values : NO_VALUES;
}

void Node::set(std::string_view id, std::string value)
{
  std::vector<std::string> values;
  values.push_back(std::move(value));
  this->setValues(id, std::move(values));
}

void Node::setValues(std::string_view id, std::vector<std::string> values)
{
  if (values.empty()) {
    this->remove(id);
    return;
  }
  for (Property& p : this->props) {
    if (p.id == id) {
      p.values = std::move(values);
      return;
    }
  }
  this->props.push_back(Property{ std::string(id), std::move(values) });
}

void Node::remove(std::string_view id)
{
  std::erase_if(this->props, [id](const Property& p) { return p.id == id; });
}

bool Node::addValue(std::string_view id, std::string value)
{
  for (Property& p : this->props) {
    if (p.id != id) continue;
    if (std::find(p.values.begin(), p.values.end(), value) != p.values.end()) return false;
    p.values.push_back(std::move(value));
    return true;
  }
  this->props.push_back(Property{ std::string(id), { std::move(value) } });
  return true;
}

bool Node::removeValue(std::string_view id, std::string_view value)
{
  for (size_t i = 0; i < this->props.size(); ++i) {
    Property& p = this->props[i];
    if (p.id != id) continue;
    const auto it = std::find(p.values.begin(), p.values.end(), value);
    if (it == p.values.end()) return false;
    p.values.erase(it);
    if (p.values.empty()) this->props.erase(this->props.begin() + ptrdiff_t(i));
    return true;
  }
  return false;
}

int Node::indexInParent() const
{
  if (!this->up) return -1;
  for (size_t i = 0; i < this->up->kids.size(); ++i) {
    if (this->up->kids[i].get() == this) return int(i);
  }
  return -1;
}

Node& Node::addChild(std::unique_ptr<Node> node, size_t index)
{
  node->up = this;
  index = std::min(index, this->kids.size());
  this->kids.insert(this->kids.begin() + ptrdiff_t(index), std::move(node));
  return *this->kids[index];
}

std::unique_ptr<Node> Node::detachChild(size_t index)
{
  std::unique_ptr<Node> node = std::move(this->kids[index]);
  this->kids.erase(this->kids.begin() + ptrdiff_t(index));
  node->up = nullptr;
  return node;
}

std::unique_ptr<Node> Node::clone() const
{
  auto copy   = std::make_unique<Node>();
  copy->props = this->props;
  copy->kids.reserve(this->kids.size());
  for (const auto& kid : this->kids) copy->addChild(kid->clone());
  return copy;
}

Collection Collection::clone() const
{
  Collection copy;
  for (const auto& game : this->games) copy.games.push_back(game->clone());
  return copy;
}

// ---------------------------------------------------------------- read

// A stack of open '('s rather than recursion: some programs write every move
// as a variation of the one before, which nests as deep as the game is long.
std::optional<Collection> Parse(std::string_view text, std::string* error)
{
  Collection result;

  // Each '(' remembers the node its first node hangs from: the one before
  // the '(' in the enclosing sequence, or none for a game at the top level.
  std::vector<Node*> open;
  Node*              attach = nullptr;
  bool               inTree = false;

  size_t i = 0;
  const auto fail = [&](const char* what) -> std::optional<Collection> {
    if (error) *error = std::string(what) + " at character " + std::to_string(i);
    return std::nullopt;
  };

  while (i < text.size()) {
    const char c = text[i];
    if (IsSpace(c)) {
      ++i;
      continue;
    }

    if (c == '(') {
      open.push_back(attach);
      inTree = true;
      ++i;
      continue;
    }

    if (c == ')') {
      if (!open.empty()) {
        attach = open.back();
        open.pop_back();
      }
      inTree = !open.empty();
      ++i;
      continue;
    }

    if (!inTree) {
      // Mail headers, a web page's leftovers: anything outside a tree is
      // not SGF, and not our business.
      ++i;
      continue;
    }

    if (c == ';') {
      auto  node = std::make_unique<Node>();
      Node* made = node.get();
      if (attach) attach->addChild(std::move(node));
      else        result.games.push_back(std::move(node));
      attach = made;
      ++i;
      continue;
    }

    if (IsUpper(c) || IsLower(c)) {
      // FF[3] and older allowed lower case letters in identifiers
      // (AddBlack for AB); they do not count.
      std::string id;
      while (i < text.size() && (IsUpper(text[i]) || IsLower(text[i]))) {
        if (IsUpper(text[i])) id += text[i];
        ++i;
      }
      std::vector<std::string> values;
      while (true) {
        while (i < text.size() && IsSpace(text[i])) ++i;
        if (i >= text.size() || text[i] != '[') break;
        const size_t start = ++i;
        while (i < text.size() && text[i] != ']') {
          if (text[i] == '\\') ++i;
          ++i;
        }
        if (i >= text.size()) return fail("Unterminated property value");
        values.push_back(Unescape(text.substr(start, i - start)));
        ++i;  // the ']'
      }
      if (!attach) return fail("Property outside a node");
      if (id.empty() || values.empty()) continue;  // a stray identifier; nothing to keep
      // A property given twice in one node, which some programs write: the
      // values join up, rather than the second quietly replacing the first.
      if (Property* existing = attach->find(id)) {
        for (std::string& v : values) existing->values.push_back(std::move(v));
      } else {
        attach->setValues(id, std::move(values));
      }
      continue;
    }

    // Anything else inside a tree is junk; skip it rather than give up on
    // an otherwise good file.
    ++i;
  }

  if (result.games.empty()) return fail("No game found");
  return result;
}

// ---------------------------------------------------------------- write

std::string Write(const Collection& collection)
{
  std::string out;

  for (const auto& game : collection.games) {
    // Each entry is a node to write and whether it starts a variation (and
    // so a '('); a null node closes one.
    struct Step {
      const Node* node;
      bool        opens;
    };
    std::vector<Step> todo{ { game.get(), true } };
    size_t lineStart = out.size();

    while (!todo.empty()) {
      const Step step = todo.back();
      todo.pop_back();

      if (!step.node) {
        out += ')';
        continue;
      }
      if (step.opens) {
        if (step.node != game.get()) {
          out += '\n';
          lineStart = out.size();
        }
        out += '(';
      }

      AppendNode(out, *step.node, step.node == game.get());
      if (step.node == game.get()) {
        out += '\n';
        lineStart = out.size();
      } else {
        BreakIfLong(out, lineStart);
      }

      const size_t kids = step.node->childCount();
      if (step.opens) todo.push_back({ nullptr, false });  // closes this variation after everything under it
      if (kids == 1) {
        todo.push_back({ &step.node->child(0), false });
      } else {
        // Reversed, so the main line comes off the stack first.
        for (size_t k = kids; k-- > 0;) todo.push_back({ &step.node->child(k), true });
      }
    }
    out += '\n';
  }
  return out;
}

// ---------------------------------------------------------------- charset

std::string DeclaredCharset(std::string_view bytes)
{
  // CA can only be in a root node, which comes first; a stray "CA[" in a
  // comment much later is not a declaration. The bytes up to it are ASCII
  // in any charset a Go file is written in.
  for (size_t at = bytes.find("CA["); at != std::string_view::npos; at = bytes.find("CA[", at + 1)) {
    // Must be an identifier of its own: not the tail of, say, "PCA[".
    if (at > 0 && (IsUpper(bytes[at - 1]) || IsLower(bytes[at - 1]))) continue;
    const size_t end = bytes.find(']', at + 3);
    if (end == std::string_view::npos) return {};
    std::string name(bytes.substr(at + 3, end - at - 3));
    std::erase_if(name, [](char c) { return IsSpace(c); });
    return name;
  }
  return {};
}

bool IsUtf8(std::string_view bytes)
{
  size_t i = 0;
  while (i < bytes.size()) {
    const unsigned char c = static_cast<unsigned char>(bytes[i]);
    int follow = 0;
    if (c < 0x80)                follow = 0;
    else if ((c & 0xE0) == 0xC0) follow = 1;
    else if ((c & 0xF0) == 0xE0) follow = 2;
    else if ((c & 0xF8) == 0xF0) follow = 3;
    else                         return false;
    if (c == 0xC0 || c == 0xC1) return false;  // overlong encodings of ASCII
    for (int k = 1; k <= follow; ++k) {
      if (i + size_t(k) >= bytes.size()) return false;
      if ((static_cast<unsigned char>(bytes[i + size_t(k)]) & 0xC0) != 0x80) return false;
    }
    i += size_t(follow) + 1;
  }
  return true;
}

}  // namespace sgf
