// SGF, the Smart Game Format (FF[4], https://www.red-bean.com/sgf/): the file
// format every Go program reads and writes. A file is a collection of game
// trees; a tree is nodes, each holding properties -- B[dd] is a black move,
// C[...] a comment, AB[aa][bb] black stones set up -- and each node may have
// any number of children, the first being the main line and the rest its
// variations.
//
// Nothing here knows what the properties mean. Every property read is kept,
// in the order it was read, and written back as it was, so a file that goes
// through the editor keeps everything in it -- including properties from
// other programs that nothing here has heard of. What the properties mean is
// game/Game.hpp's business.

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgf {

struct Property {
  std::string              id;      // upper case, as FF[4] spells them
  std::vector<std::string> values;  // with the escapes taken out
};

class Node {
public:
  Node() = default;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  // --- properties ---
  const std::vector<Property>& properties() const { return this->props; }

  bool            has(std::string_view id) const { return this->find(id) != nullptr; }
  const Property* find(std::string_view id) const;
  Property*       find(std::string_view id);

  // The first value, or "" when the property is missing.
  const std::string& get(std::string_view id) const;
  // Every value, or none when the property is missing.
  const std::vector<std::string>& values(std::string_view id) const;

  // Replaces the property with this one value. An empty value is still a
  // value -- B[] is a pass -- so use remove() to take a property away.
  void set(std::string_view id, std::string value);
  // Replaces every value; no values removes the property.
  void setValues(std::string_view id, std::vector<std::string> values);
  void remove(std::string_view id);

  // For list properties like AB or TR. False if nothing changed: the value
  // was already there, or was not there to remove.
  bool addValue(std::string_view id, std::string value);
  bool removeValue(std::string_view id, std::string_view value);

  // --- tree ---
  Node*  parent() const { return this->up; }
  size_t childCount() const { return this->kids.size(); }
  Node&  child(size_t index) const { return *this->kids[index]; }
  // Where this node is among its parent's children, or -1 for a root.
  int    indexInParent() const;

  // `index` past the end appends.
  Node&                 addChild(std::unique_ptr<Node> node, size_t index = size_t(-1));
  std::unique_ptr<Node> detachChild(size_t index);

  // A deep copy, with no parent.
  std::unique_ptr<Node> clone() const;

private:
  std::vector<Property>              props;
  std::vector<std::unique_ptr<Node>> kids;
  Node*                              up = nullptr;
};

// A whole file: usually one game, but a collection may hold several.
struct Collection {
  std::vector<std::unique_ptr<Node>> games;

  Collection clone() const;
};

// Parses a whole file, already converted to UTF-8. Lenient where real files
// are sloppy -- text before the first '(', a missing ')' at the end, FF[3]
// identifiers with lower case letters in them -- and nullopt only when there
// is no game in it at all, with the reason in `error`.
std::optional<Collection> Parse(std::string_view text, std::string* error = nullptr);

// The collection as SGF text, UTF-8.
std::string Write(const Collection& collection);

// The CA[] charset a file declares, found in the raw bytes before anything is
// converted, or "" if it declares none. Parsing has to wait for the answer:
// in Shift_JIS or Big5 a character's second byte can be ']' or '\'.
std::string DeclaredCharset(std::string_view bytes);

// True if `bytes` is well-formed UTF-8.
bool IsUtf8(std::string_view bytes);

}  // namespace sgf
