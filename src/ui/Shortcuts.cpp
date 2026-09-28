#include <ui/Shortcuts.hpp>

#include <raylib.h>

#include <algorithm>

namespace ui {

const std::vector<Shortcut>& AllShortcuts()
{
  static const std::vector<Shortcut> shortcuts = {
    { KEY_LEFT,      false, false, Command::Back,              true,  "Left",          "Back one move" },
    { KEY_RIGHT,     false, false, Command::Forward,           true,  "Right",         "Forward one move" },
    { KEY_PAGE_UP,   false, false, Command::BackMany,          true,  "Page Up",       "Back ten moves" },
    { KEY_PAGE_DOWN, false, false, Command::ForwardMany,       true,  "Page Down",     "Forward ten moves" },
    { KEY_HOME,      false, false, Command::Start,             false, "Home",          "To the start" },
    { KEY_END,       false, false, Command::End,               false, "End",           "To the end of the line" },
    { KEY_UP,        false, false, Command::PreviousVariation, true,  "Up",            "Previous variation" },
    { KEY_DOWN,      false, false, Command::NextVariation,     true,  "Down",          "Next variation" },
    { KEY_DELETE,    false, false, Command::DeleteBranch,      false, "Delete",        "Delete this move and everything after it" },
    { KEY_P,         true,  false, Command::Pass,              false, "Ctrl+P",        "Pass" },
    { KEY_M,         true,  false, Command::PromoteMainLine,   false, "Ctrl+M",        "Make this line the main line" },
    { KEY_Z,         true,  false, Command::Undo,              true,  "Ctrl+Z",        "Undo" },
    { KEY_Y,         true,  false, Command::Redo,              true,  "Ctrl+Y",        "Redo" },
    { KEY_Z,         true,  true,  Command::Redo,              true,  "Ctrl+Shift+Z",  "Redo" },
    { KEY_N,         true,  false, Command::NewGame,           false, "Ctrl+N",        "New game" },
    { KEY_O,         true,  false, Command::Open,              false, "Ctrl+O",        "Open a file" },
    { KEY_S,         true,  false, Command::Save,              false, "Ctrl+S",        "Save" },
    { KEY_S,         true,  true,  Command::SaveAs,            false, "Ctrl+Shift+S",  "Save as" },
    { KEY_I,         true,  false, Command::GameInfo,          false, "Ctrl+I",        "Game information" },
    { KEY_F1,        false, false, Command::Help,              false, "F1",            "This help" },
    { KEY_ESCAPE,    false, false, Command::Cancel,            false, "Esc",           "Close a page, leave a text box, or back to the Play tool" },
    { KEY_Q,         false, false, ToolCommand(Tool::Play),     false, "Q",            "Tool: play moves" },
    { KEY_B,         false, false, ToolCommand(Tool::Black),    false, "B",            "Tool: set up black stones" },
    { KEY_W,         false, false, ToolCommand(Tool::White),    false, "W",            "Tool: set up white stones" },
    { KEY_E,         false, false, ToolCommand(Tool::Erase),    false, "E",            "Tool: erase set-up stones" },
    { KEY_T,         false, false, ToolCommand(Tool::Triangle), false, "T",            "Tool: triangle" },
    { KEY_S,         false, false, ToolCommand(Tool::Square),   false, "S",            "Tool: square" },
    { KEY_C,         false, false, ToolCommand(Tool::Circle),   false, "C",            "Tool: circle" },
    { KEY_X,         false, false, ToolCommand(Tool::Cross),    false, "X",            "Tool: cross" },
    { KEY_L,         false, false, ToolCommand(Tool::Letter),   false, "L",            "Tool: letters" },
    { KEY_N,         false, false, ToolCommand(Tool::Number),   false, "N",            "Tool: numbers" },
    { KEY_A,         false, false, ToolCommand(Tool::Arrow),    false, "A",            "Tool: arrows" },
  };
  return shortcuts;
}

std::vector<Command> Shortcuts::poll(bool typing, bool dialog)
{
  this->taken.clear();
  std::vector<Command> commands;

  const bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  const bool alt   = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
  if (alt) return commands;  // Alt+F4 and the like are Windows'

  for (const Shortcut& s : AllShortcuts()) {
    if (s.ctrl != ctrl || s.shift != shift) continue;

    const bool pressed = IsKeyPressed(s.key) || (s.repeats && IsKeyPressedRepeat(s.key));
    if (!pressed) continue;

    const bool always = s.ctrl || (s.key >= KEY_F1 && s.key <= KEY_F12) || s.command == Command::Cancel;
    if (typing && !always) continue;
    if (dialog && s.command != Command::Cancel) continue;

    commands.push_back(s.command);
    // Esc stays the Gui's as well: a drop-down that is open closes on it.
    if (s.command != Command::Cancel) this->taken.push_back(s.key);
  }
  return commands;
}

bool Shortcuts::claimed(int key) const
{
  return std::find(this->taken.begin(), this->taken.end(), key) != this->taken.end();
}

}  // namespace ui
