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
// Which keys do what is the player's to choose (see Controls). The one
// exception is a text box with the caret in it: there the plain keys are the
// text's, and so are Ctrl with A, C, V and X, so only the other combinations
// with Ctrl or Alt, the function keys and Esc -- which lets go of the text
// box -- still work.

#pragma once

#include <ui/Commands.hpp>
#include <ui/Controls.hpp>

#include <optional>
#include <vector>

namespace ui {

class Shortcuts {
public:
  // Reads this frame's key presses. `typing` means a text box has the caret;
  // `dialog` that a page is up over the editor, where only Esc (Cancel) and
  // the interface scale apply.
  std::vector<Command> poll(const Bindings& bindings, bool typing, bool dialog);

  // For the Controls page, waiting for the keys to bind: the first key
  // pressed this frame that can be bound, with the modifiers held with it,
  // or nothing. The key is claimed, so nothing else sees it.
  std::optional<KeyCombo> capture();

  // Whether the Gui should be kept from seeing this raylib key this frame.
  bool claimed(int key) const;

private:
  std::vector<int> taken;
};

}  // namespace ui
