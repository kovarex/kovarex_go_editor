// Everything the editor can be asked to do from a button or a key, so the two
// always do the same thing: the side panel's buttons and the keyboard
// shortcuts both come down to one of these.

#pragma once

// What a click on the board does.
enum class Tool {
  Play,            // moves, each side in turn
  Black,           // set-up stones
  White,
  Erase,           // clears set-up stones
  Triangle,
  Square,
  Circle,
  Cross,
  Selected,
  Letter,          // A, B, C... in turn
  Number,          // 1, 2, 3... in turn
  Text,            // a label of whatever is typed in the tool bar
  Arrow,           // click the start, then the end
  Line,
  TerritoryBlack,
  TerritoryWhite,
  Dim,
  Pen,             // draws on the board, to show something: not part of the game
  Count
};

enum class Command {
  None,

  // Moving about the record.
  Back,
  Forward,
  BackMany,
  ForwardMany,
  Start,
  End,
  PreviousVariation,
  NextVariation,

  // Changing it.
  Pass,
  DeleteBranch,
  PromoteMainLine,
  Undo,
  Redo,
  // The current move and everything after it, through the clipboard.
  Cut,
  Copy,
  Paste,
  // The whole game turned a quarter, or mirrored.
  RotateLeft,
  RotateRight,
  FlipHorizontal,
  FlipVertical,

  // Files and pages.
  NewGame,
  Open,
  Save,
  SaveAs,
  GameInfo,
  AiSensei,  // opens AI Sensei's upload page in the browser, the game on it
  Settings,
  Controls,
  About,
  Online,  // the Online page: sharing the game with other editors

  // Opens the search of the page that is up, or puts the caret back in it.
  FocusSearch,

  // Esc: closes a page, lets go of a text box, or drops a half-drawn arrow.
  Cancel,

  // The interface scale a step bigger or smaller -- from the automatic one,
  // which they leave for a manual one -- or back to automatic.
  ScaleUp,
  ScaleDown,
  ScaleAutomatic,

  // Picking a tool; ToolFirst + int(tool).
  ToolFirst,
  ToolLast = ToolFirst + int(Tool::Count) - 1,
};

constexpr Command ToolCommand(Tool tool)
{
  return Command(int(Command::ToolFirst) + int(tool));
}

constexpr bool IsToolCommand(Command c)
{
  return int(c) >= int(Command::ToolFirst) && int(c) <= int(Command::ToolLast);
}

constexpr Tool CommandTool(Command c)
{
  return Tool(int(c) - int(Command::ToolFirst));
}
