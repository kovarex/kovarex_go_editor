// The keyboard shortcuts, read straight from the keyboard ahead of the Gui.
//
// A widget library hands keys to whichever widget has the focus, and that
// is how shortcuts go missing in editors built on one: click a button, the
// button has the focus, and the arrow keys now belong to it. So shortcuts
// are not Gui events here at all. Each frame, before the Gui looks at the
// keyboard, this reads it; a key that makes a shortcut is claimed, and the
// Gui is never told it was pressed (see RaylibInput::setKeyFilter). Whatever
// has the focus, a shortcut does the same thing.
//
// The one exception is a text box with the caret in it: there the arrows,
// Delete, letters and cut, copy and paste are for the text, so only the other
// shortcuts with Ctrl, the function keys and Esc -- which lets go of the text
// box -- still work.

#pragma once

#include <ui/Commands.hpp>

#include <vector>

namespace ui {

struct Shortcut {
  int         key;  // raylib KEY_*
  bool        ctrl;
  bool        shift;
  Command     command;
  bool        repeats;  // held down, it goes again, like stepping through a game
  const char* keys;     // for the Help page
  const char* what;
};

// Every shortcut, in the order the Help page lists them.
const std::vector<Shortcut>& AllShortcuts();

class Shortcuts {
public:
  // Reads this frame's key presses. `typing` means a text box has the caret;
  // `dialog` that a page is up over the editor, where only Esc and the
  // file shortcuts apply.
  std::vector<Command> poll(bool typing, bool dialog);

  // Whether the Gui should be kept from seeing this raylib key this frame.
  bool claimed(int key) const;

private:
  std::vector<int> taken;
};

}  // namespace ui
