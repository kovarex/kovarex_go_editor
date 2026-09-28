#include <ui/Shortcuts.hpp>

#include <raylib.h>

#include <algorithm>

namespace ui {

namespace {

KeyCombo Held(int key)
{
  return { key, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
           IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT), IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) };
}

// Whether a text box with the caret in it leaves `combo` to the shortcuts.
bool TextBoxLetsGo(const KeyCombo& combo)
{
  if (combo.key == KEY_ESCAPE || (combo.key >= KEY_F1 && combo.key <= KEY_F12)) return true;
  if (combo.alt) return true;
  if (!combo.ctrl) return false;
  // Select all, and the text's own cut, copy and paste.
  return combo.key != KEY_A && combo.key != KEY_C && combo.key != KEY_V && combo.key != KEY_X && combo.key != KEY_INSERT;
}

}  // namespace

std::vector<Command> Shortcuts::poll(const Bindings& bindings, bool typing, bool dialog)
{
  this->taken.clear();
  std::vector<Command> commands;

  const std::vector<Control>& controls = AllControls();
  for (size_t i = 0; i < controls.size() && i < bindings.keys.size(); ++i) {
    const Control& control = controls[i];
    for (const KeyCombo& combo : { bindings.keys[i].primary, bindings.keys[i].alternative }) {
      if (!combo.isSet() || Held(combo.key) != combo) continue;
      // Alt+F4 is Windows', whatever is bound to it.
      if (combo.alt && combo.key == KEY_F4) continue;

      const bool pressed = IsKeyPressed(combo.key) || (control.repeats && IsKeyPressedRepeat(combo.key));
      if (!pressed) continue;
      // A key two controls share goes to the first.
      if (this->claimed(combo.key)) continue;

      if (typing && !TextBoxLetsGo(combo)) continue;
      const bool onPages = control.command == Command::Cancel || control.command == Command::ScaleUp ||
                           control.command == Command::ScaleDown || control.command == Command::ScaleAutomatic;
      if (dialog && !onPages) continue;

      commands.push_back(control.command);
      this->taken.push_back(combo.key);
      break;
    }
  }
  // Esc stays the Gui's as well: a drop-down that is open closes on it.
  std::erase(this->taken, KEY_ESCAPE);
  return commands;
}

std::optional<KeyCombo> Shortcuts::capture()
{
  this->taken.clear();
  std::optional<KeyCombo> result;
  // Drains the queue, so the keys pressed while waiting don't turn up later.
  for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
    if (result || !IsBindable(key)) continue;
    result = Held(key);
    this->taken.push_back(key);
  }
  return result;
}

bool Shortcuts::claimed(int key) const
{
  return std::find(this->taken.begin(), this->taken.end(), key) != this->taken.end();
}

}  // namespace ui
