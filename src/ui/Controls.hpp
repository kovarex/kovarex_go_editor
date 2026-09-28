// The controls: every command a key can give, and the keys that give it.
//
// As in Factorio, each control has two bindings, the primary one and an
// alternative, either of which may be unset. What they are by default is
// part of the table here; what the player made of them is kept in
// config.ini (see Settings) and changed on the Controls page.

#pragma once

#include <ui/Commands.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace ui {

// A key with the modifiers held along with it. `key` is a raylib KEY_*, or
// 0 when the binding isn't set.
struct KeyCombo {
  int  key   = 0;
  bool ctrl  = false;
  bool shift = false;
  bool alt   = false;

  bool isSet() const { return this->key != 0; }
  bool operator==(const KeyCombo&) const = default;
};

// "Ctrl + Shift + Z", or "Not set". ParseKey() reads it back, as written in
// config.ini; anything it doesn't know is not set.
std::string KeyText(const KeyCombo& combo);
KeyCombo    ParseKey(std::string_view text);

// Keys as Factorio shows them in a tooltip: in control_input_shortcut_label's
// semibold light blue, as rich text (see agui_raylib::RegisterFont).
std::string ShortcutText(std::string_view keys);

// Whether a key can be bound at all: one the editor has a name for, and not
// a modifier, which only goes with another key.
bool IsBindable(int key);

// The groups the Controls page shows the controls in.
enum class ControlSection { Moving, Editing, Tools, Files, Interface };

struct Control {
  const char*    id;       // its key in config.ini
  Command        command;
  const char*    name;     // on the Controls page
  const char*    tip;      // its tooltip there, if it needs one
  ControlSection section;
  bool           repeats;  // held down, it goes again, like stepping through a game
  KeyCombo       primary;  // the defaults
  KeyCombo       alternative;
};

// Every control, in the order the Controls page lists them.
const std::vector<Control>& AllControls();

// The keys bound to each of AllControls(), by its index there.
struct Bindings {
  struct Pair {
    KeyCombo primary;
    KeyCombo alternative;
    bool operator==(const Pair&) const = default;
  };
  std::vector<Pair> keys;

  // The defaults.
  Bindings();

  // The first binding set for `command`, as "Ctrl + Z", or empty when none is.
  std::string keysFor(Command command) const;

  bool operator==(const Bindings&) const = default;
};

}  // namespace ui
