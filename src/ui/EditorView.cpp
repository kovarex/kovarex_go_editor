#include <ui/EditorView.hpp>

#include <ui/Form.hpp>
#include <ui/GoSprites.hpp>
#include <ui/Theme.hpp>

#include <Agui/Gui.hpp>
#include <Agui/SystemClipboard.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/HorizontalFlow.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/TextBox.hpp>
#include <Agui/Widget/TextField.hpp>
#include <Agui/Widget/VerticalFlow.hpp>

#include <algorithm>
#include <cstdlib>

namespace ui {

namespace {

// The tool buttons' size: Theme::toolButton's.
constexpr int TOOL_PX = Theme::TOOL_PX;
// The side panel's width. Nine tool buttons across, and the players' names.
constexpr int SIDE_W = 432;
// Space kept round the board.
constexpr int BOARD_GAP = 8;
// How long a message stays on the status line.
constexpr float STATUS_SECONDS = 6.0f;
// Below this, the comment and the tree stop shrinking and the panel runs off
// the bottom of the window instead.
constexpr int MIN_COMMENT_H = 60;
constexpr int MIN_TREE_H    = 60;

struct ToolLook {
  Tool                  tool;
  std::optional<Sprite> icon;
  const char*           text;
  const char*           tip;
};

// The tool bar, in order: two rows of nine.
constexpr ToolLook TOOLS[] = {
  { Tool::Play,           std::nullopt,           "",    "Play moves (Q)\nCtrl+click inserts a move after this one; drag a stone to move it, however long ago it was played." },
  { Tool::Black,          Sprite::BlackStone,     "",    "Set up black stones (B)" },
  { Tool::White,          Sprite::WhiteStone,     "",    "Set up white stones (W)" },
  { Tool::Erase,          std::nullopt,           "Clr", "Clear set-up stones (E)" },
  { Tool::Triangle,       Sprite::TriangleDark,   "",    "Triangle (T)" },
  { Tool::Square,         Sprite::SquareDark,     "",    "Square (S)" },
  { Tool::Circle,         Sprite::CircleDark,     "",    "Circle (C)" },
  { Tool::Cross,          Sprite::CrossDark,      "",    "Cross (X)" },
  { Tool::Selected,       Sprite::Selected,       "",    "Selected points (SL)" },
  { Tool::Letter,         std::nullopt,           "A",   "Letters, A, B, C... (L)" },
  { Tool::Number,         std::nullopt,           "1",   "Numbers, 1, 2, 3... (N)" },
  { Tool::Text,           std::nullopt,           "Ab",  "A label of your own: type it in the box on the right" },
  { Tool::Arrow,          std::nullopt,           "->",  "Arrow: click where it starts, then where it points (A)" },
  { Tool::Line,           std::nullopt,           "--",  "Line: click one end, then the other" },
  { Tool::TerritoryBlack, Sprite::TerritoryBlack, "",    "Black's territory" },
  { Tool::TerritoryWhite, Sprite::TerritoryWhite, "",    "White's territory" },
  { Tool::Dim,            std::nullopt,           "Dim", "Dim points, to put them in the background" },
};

// The drop-downs' items, and the SGF property each sets.
constexpr const char* POSITION_NOTES[][2] = {
  { "Position: -", "" }, { "Good for Black", "GB" }, { "Good for White", "GW" }, { "Even", "DM" }, { "Unclear", "UC" },
};
constexpr const char* MOVE_NOTES[][2] = {
  { "Move: -", "" }, { "Good move", "TE" }, { "Bad move", "BM" }, { "Interesting", "IT" }, { "Doubtful", "DO" },
};

void Pin(agui::Widget& widget, agui::Style& style, int w, int h)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
  style.setMinimalHeight(h);
  style.setMaximalHeight(h);
  widget.setSize(w, h, agui::SetSizeInfo());
}

void PinWidth(agui::Style& style, int w)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
}

agui::ImageWidget& Icon(const GoSprites& sprites, Sprite sprite, int side)
{
  agui::ImageWidget& icon = make<agui::ImageWidget>(sprites.image(sprite));
  icon.scaleToKeepTheRatio = true;
  icon.setIgnoredByInteraction(true);
  Pin(icon, icon.style, side, side);
  return icon;
}

// "Honinbo Shusaku 4d", from a name and a rank either of which may be missing.
std::string Player(const sgf::Node& root, const char* name, const char* rank, const char* fallback)
{
  std::string text = root.get(name).empty() ? fallback : root.get(name);
  if (!root.get(rank).empty()) text += "  " + root.get(rank);
  return text;
}

}  // namespace

// ---------------------------------------------------------------- building

EditorView::EditorView(agui::Gui& gui, Theme& theme, const GoSprites& sprites)
    : gui(gui)
    , theme(theme)
    , sprites(sprites)
    , board(theme, sprites)
    , tree(theme, sprites)
    , topBar(agui::GuiDirection::Horizontal)
    , side(agui::GuiDirection::Vertical)
{
  this->topBar << this->buildTopBar();

  agui::VerticalFlow& upperFlow = column(8);
  upperFlow << this->buildPlayers();
  upperFlow << this->buildTools();
  upperFlow << this->buildNavigation();
  upperFlow << this->buildNode();
  this->upper = &upperFlow;
  this->side << upperFlow;

  this->comment = &make<agui::TextBox>();
  this->comment->setWordWrap(true);
  this->comment->setHScrollPolicy(agui::ScrollPolicy::Never);
  this->comment->setVScrollPolicy(agui::ScrollPolicy::Auto);
  this->comment->setToolTip("The comment on this move. Esc, or a click on the board, and the keys are shortcuts again.");
  this->comment->onTextEdit(this, [this] {
    if (this->game) this->game->setNodeText("C", this->comment->getText());
  });
  this->side << *this->comment;

  this->side << this->tree.widget();

  // Long enough to say why an edit was refused, so it wraps.
  this->status = &agui::label("", &theme.dimLabel, agui::SingleLine::False);
  this->side << *this->status;

  this->board.onOutcome = [this](const Outcome& outcome) { this->report(outcome); };
  this->board.onWheel   = [this](int steps) { this->pending.push_back(steps < 0 ? Command::Back : Command::Forward); };

  gui.add(&this->board.widget());
  gui.add(&this->topBar);
  gui.add(&this->side);
  this->setTool(Tool::Play);
}

EditorView::~EditorView()
{
  this->gui.remove(&this->side);
  this->gui.remove(&this->topBar);
  this->gui.remove(&this->board.widget());
}

agui::Button& EditorView::commandButton(const char* text, Command command, const char* tip, int width)
{
  agui::Button& button = agui::button(std::string(text), &this->side, [this, command] { this->pending.push_back(command); },
                                      &this->theme.smallButton);
  button.setFocusable(false);
  if (tip) button.setToolTip(tip);
  if (width > 0) button.style.setMinimalWidth(width);
  return button;
}

agui::Widget& EditorView::buildTopBar()
{
  agui::HorizontalFlow& bar = row(4);
  bar.style.setHorizontallyStretchable(true);
  bar << this->commandButton("New", Command::NewGame, "A new game (Ctrl+N)", 60);
  bar << this->commandButton("Open", Command::Open, "Open a file (Ctrl+O)", 60);
  bar << this->commandButton("Save", Command::Save, "Save (Ctrl+S)", 60);
  bar << this->commandButton("Save as", Command::SaveAs, "Save under another name (Ctrl+Shift+S)", 80);
  bar << this->commandButton("Game info", Command::GameInfo, "Players, result, date and the rest (Ctrl+I)", 100);
  bar << this->commandButton("AI Sensei", Command::AiSensei,
                             "Have AI Sensei review this game: its upload page opens in the browser with the game on it", 100);

  this->title = &agui::label("", &this->theme.headingLabel);
  this->title->style.setLeftPadding(16);
  bar << *this->title;
  bar << agui::pusher;
  bar << this->commandButton("Help", Command::Help, "What the keys and clicks do (F1)", 60);
  bar << this->commandButton("Settings", Command::Settings, nullptr, 90);
  return bar;
}

agui::Widget& EditorView::buildPlayers()
{
  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.insideShallowFrameWithPadding);
  agui::VerticalFlow& rows = column(4);

  const auto playerRow = [this](Sprite stone, agui::Label*& name, agui::Label*& captures) {
    agui::HorizontalFlow& r = row(8);
    r.style.setHorizontallyStretchable(true);
    name     = &agui::label("", &this->theme.headingLabel);
    captures = &agui::label("", &this->theme.dimLabel);
    captures->setToolTip("Stones this player has captured");
    r << Icon(this->sprites, stone, 20) << *name << agui::pusher << *captures;
    return &r;
  };
  rows << *playerRow(Sprite::BlackStone, this->blackName, this->blackCaptures);
  rows << *playerRow(Sprite::WhiteStone, this->whiteName, this->whiteCaptures);

  // The move and whose turn it is; under it, the variations from here and
  // the game's komi, handicap and result. (Not its size: the board shows that.)
  agui::HorizontalFlow& move = row(8);
  move.style.setHorizontallyStretchable(true);
  this->moveLabel = &agui::label("");
  this->turnLabel = &agui::label("");
  move << *this->moveLabel << agui::pusher << *this->turnLabel;
  rows << move;

  agui::HorizontalFlow& about = row(8);
  about.style.setHorizontallyStretchable(true);
  this->variationsLabel = &agui::label("", &this->theme.dimLabel);
  this->gameLabel       = &agui::label("", &this->theme.dimLabel);
  about << *this->variationsLabel << agui::pusher << *this->gameLabel;
  rows << about;

  panel << rows;
  return panel;
}

agui::Button& EditorView::toolButton(Tool which)
{
  const ToolLook& look = *std::find_if(std::begin(TOOLS), std::end(TOOLS), [which](const ToolLook& t) { return t.tool == which; });
  agui::Button& button = look.text[0] ? agui::button(std::string(look.text), &this->side, nullptr, &this->theme.toolButton)
                                      : make<agui::Button>(&this->theme.toolButton);
  button.setFocusable(false);
  button.setToggleButton(true);
  button.setToolTip(look.tip);
  button.onClick(this, [this, which] { this->pending.push_back(ToolCommand(which)); });

  // Pictures sit in the middle of the button. A button does not lay out
  // its children, so they are placed by hand -- see placeIcons().
  constexpr int ICON = 24;
  const int     at   = (TOOL_PX - ICON) / 2;
  if (look.icon) {
    agui::ImageWidget& icon = Icon(this->sprites, *look.icon, ICON);
    button << icon;
    this->icons.push_back({ &button, &icon, at, at });
  } else if (which == Tool::Play) {
    // Both colours, one after the other, the pair centred together.
    constexpr int STONE = 16, STEP = 8;
    const int     from  = (TOOL_PX - STONE - STEP) / 2;
    agui::ImageWidget& black = Icon(this->sprites, Sprite::BlackStone, STONE);
    agui::ImageWidget& white = Icon(this->sprites, Sprite::WhiteStone, STONE);
    button << black << white;
    this->icons.push_back({ &button, &black, from, from });
    this->icons.push_back({ &button, &white, from + STEP, from + STEP });
  }
  this->toolButtons[size_t(which)] = &button;
  return button;
}

agui::Frame& EditorView::group(const char* caption)
{
  agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.borderedFrame);
  frame << agui::label(caption, &this->theme.captionLabel);
  return frame;
}

// The tools, in groups: stones, territory, and the marks and labels.
agui::Widget& EditorView::buildTools()
{
  agui::VerticalFlow& rows = column(4);

  agui::HorizontalFlow& stones = row(4);
  for (Tool t : { Tool::Play, Tool::Black, Tool::White, Tool::Erase }) stones << this->toolButton(t);
  agui::HorizontalFlow& territory = row(4);
  for (Tool t : { Tool::TerritoryBlack, Tool::TerritoryWhite }) territory << this->toolButton(t);
  agui::HorizontalFlow& top = row(4);
  top.style.setHorizontallyStretchable(true);
  top << (this->group("Stones") << stones) << (this->group("Territory") << territory);
  rows << top;

  agui::HorizontalFlow& shapes = row(4);
  for (Tool t : { Tool::Triangle, Tool::Square, Tool::Circle, Tool::Cross, Tool::Selected, Tool::Dim }) {
    shapes << this->toolButton(t);
  }
  agui::HorizontalFlow& labels = row(4);
  for (Tool t : { Tool::Letter, Tool::Number, Tool::Text }) labels << this->toolButton(t);
  // The Text tool's text, right after its button.
  this->labelText = &make<agui::TextField>();
  this->labelText->style.setMinimalWidth(60);
  this->labelText->style.setMaximalWidth(60);
  this->labelText->setToolTip("What the Text tool writes on the board");
  this->labelText->onTextEdit(this, [this] { this->setTool(Tool::Text); });
  labels << *this->labelText;
  for (Tool t : { Tool::Arrow, Tool::Line }) labels << this->toolButton(t);
  rows << (this->group("Marks") << shapes << labels);
  return rows;
}

// Going through the record, and changing it.
agui::Widget& EditorView::buildNavigation()
{
  agui::VerticalFlow& rows = column(4);

  agui::HorizontalFlow& moves = row(4);
  // Ten moves at a time and the variations are keys only: Page Up and Down,
  // Up and Down.
  moves << this->commandButton("|<", Command::Start, "To the start (Home)");
  moves << this->commandButton("<", Command::Back, "Back one move (Left, or the mouse wheel)", 44);
  moves << this->commandButton(">", Command::Forward, "Forward one move (Right, or the mouse wheel)", 44);
  moves << this->commandButton(">|", Command::End, "To the end of this line (End)");
  agui::HorizontalFlow& edits = row(4);
  edits << this->commandButton("Pass", Command::Pass, "Pass (Ctrl+P)");
  edits << this->commandButton("Undo", Command::Undo, "Undo (Ctrl+Z)");
  edits << this->commandButton("Redo", Command::Redo, "Redo (Ctrl+Y)");
  agui::HorizontalFlow& top = row(4);
  top.style.setHorizontallyStretchable(true);
  top << (this->group("Navigate") << moves) << (this->group("Moves") << edits);
  rows << top;

  agui::HorizontalFlow& variation = row(4);
  variation << this->commandButton("Delete", Command::DeleteBranch, "Delete this move and everything after it (Delete)");
  variation << this->commandButton("Main line", Command::PromoteMainLine, "Make this line the main line (Ctrl+M)");
  variation << this->commandButton("Cut", Command::Cut, "Cut this move and everything after it (Ctrl+X)");
  variation << this->commandButton("Copy", Command::Copy, "Copy this move and everything after it (Ctrl+C)");
  variation << this->commandButton("Paste", Command::Paste, "Paste what was cut or copied as a new variation here (Ctrl+V)");
  rows << (this->group("Variation") << variation);
  return rows;
}

agui::Widget& EditorView::buildNode()
{
  agui::HorizontalFlow& r = row(4);

  this->nodeName = &make<agui::TextField>();
  PinWidth(this->nodeName->style, 104);
  this->nodeName->setToolTip("A name for this move or position (N)");
  this->nodeName->onTextEdit(this, [this] {
    if (this->game) this->game->setNodeText("N", this->nodeName->getText());
  });

  this->positionNote = &make<agui::DropDown>();
  for (const auto& note : POSITION_NOTES) this->positionNote->addItem(std::string(note[0]));
  PinWidth(this->positionNote->style, 160);
  this->positionNote->onItemSelect(this, [this](int i) {
    if (this->game && i >= 0) this->game->setPositionAnnotation(POSITION_NOTES[i][1]);
  });

  this->moveNote = &make<agui::DropDown>();
  for (const auto& note : MOVE_NOTES) this->moveNote->addItem(std::string(note[0]));
  PinWidth(this->moveNote->style, 140);
  this->moveNote->onItemSelect(this, [this](int i) {
    if (this->game && i >= 0) this->game->setMoveAnnotation(MOVE_NOTES[i][1]);
  });

  r << *this->nodeName << *this->positionNote << *this->moveNote;
  return r;
}

// ---------------------------------------------------------------- running

void EditorView::show(Game* newGame)
{
  this->game          = newGame;
  this->shownRevision = 0;
  this->commentNode   = nullptr;
  this->board.show(newGame);
  this->tree.show(newGame);
  this->refresh();
}

void EditorView::setTitle(const std::string& text)
{
  if (this->title->getText() != text) this->title->setText(text);
}

void EditorView::setBoardOptions(const BoardView::Options& options)
{
  this->board.setOptions(options);
}

void EditorView::setTreeNumbers(bool on)
{
  this->tree.setNumbers(on);
}

std::vector<Command> EditorView::takeCommands()
{
  std::vector<Command> commands;
  commands.swap(this->pending);
  return commands;
}

void EditorView::setTool(Tool newTool)
{
  this->tool = newTool;
  for (size_t i = 0; i < this->toolButtons.size(); ++i) {
    agui::Button* button = this->toolButtons[i];
    if (button && button->isToggledInternal() != (Tool(i) == newTool)) button->setToggleState(Tool(i) == newTool);
  }
  this->board.setTool(newTool, this->labelText ? this->labelText->getText() : std::string());
}

void EditorView::run(Command command)
{
  if (IsToolCommand(command)) {
    this->setTool(CommandTool(command));
    return;
  }
  if (command == Command::Cancel) {
    // Esc lets go of a text box first, then of a half-drawn arrow, then goes
    // back to playing moves.
    if (this->typing()) this->gui.clearFocus();
    else {
      this->board.cancel();
      this->setTool(Tool::Play);
    }
    return;
  }
  if (!this->game) return;

  switch (command) {
  case Command::Back:              this->game->back(); break;
  case Command::Forward:           this->game->forward(); break;
  case Command::BackMany:          this->game->step(-10); break;
  case Command::ForwardMany:       this->game->step(10); break;
  case Command::Start:             this->game->toStart(); break;
  case Command::End:               this->game->toEnd(); break;
  case Command::PreviousVariation:
    if (!this->game->previousVariation()) this->message("This is the first variation here.", false);
    break;
  case Command::NextVariation:
    if (!this->game->nextVariation()) this->message("This is the last variation here.", false);
    break;
  case Command::Pass:            this->report(this->game->pass()); break;
  case Command::DeleteBranch:    this->report(this->game->deleteBranch()); break;
  case Command::PromoteMainLine: this->report(this->game->promoteToMainLine()); break;
  case Command::Undo:
    if (!this->game->undo()) this->message("Nothing to undo.", false);
    break;
  case Command::Redo:
    if (!this->game->redo()) this->message("Nothing to redo.", false);
    break;
  case Command::Copy:
    agui::SystemClipboard::copy(this->game->copyBranch());
    this->message(this->game->current().parent() ? "Copied this move and everything after it." : "Copied the whole game.");
    break;
  case Command::Cut:
    // Nothing is cut that can't be deleted: the start of the game can't.
    if (!this->game->current().parent()) {
      this->message("The start of the game can't be cut; Ctrl+C copies the whole game.", false);
      break;
    }
    agui::SystemClipboard::copy(this->game->copyBranch());
    this->report(this->game->deleteBranch());
    this->message("Cut this move and everything after it.");
    break;
  case Command::Paste: {
    const Outcome outcome = this->game->pasteBranch(agui::SystemClipboard::paste(false));
    if (outcome.ok && outcome.message.empty()) this->message("Pasted as a new variation.");
    else                                       this->report(outcome);
    break;
  }
  default:
    break;
  }
}

void EditorView::report(const Outcome& outcome)
{
  if (!outcome.ok) this->message(outcome.message, false);
  else if (!outcome.message.empty()) this->message(outcome.message, true);
}

void EditorView::message(const std::string& text, bool good)
{
  this->status->style.setParent(good ? &this->theme.goodLabel : &this->theme.badLabel);
  this->status->setText(text);
  this->statusTime = STATUS_SECONDS;
}

bool EditorView::typing() const
{
  const agui::Widget* focused = this->gui.getFocusedWidget();
  return focused && focused->isTextBox();
}

void EditorView::setInteractive(bool value)
{
  this->board.setInteractive(value);
  this->topBar.setIgnoredByInteraction(!value);
  this->side.setIgnoredByInteraction(!value);
}

void EditorView::update(float dt, int screenWidth, int screenHeight)
{
  if (this->statusTime > 0.0f) {
    this->statusTime -= dt;
    if (this->statusTime <= 0.0f) this->status->setText(std::string());
  }

  // The Text tool writes whatever the box says now.
  if (this->tool == Tool::Text) this->board.setTool(Tool::Text, this->labelText->getText());

  this->refresh();
  this->placeIcons();
  this->board.refresh();
  this->tree.refresh();
  this->layout(screenWidth, screenHeight);
}

void EditorView::placeIcons()
{
  // Children sit inside the button's padding, and that includes the border
  // of whatever it is drawn with at the moment -- which a toggled button
  // draws differently. So the offset from the button's edge is kept, and the
  // padding taken off whatever it is now.
  for (const IconSpot& spot : this->icons) {
    const int x = spot.x - spot.button->getLeftPadding();
    const int y = spot.y - spot.button->getTopPadding();
    if (spot.icon->getLocation().x != x || spot.icon->getLocation().y != y) spot.icon->setLocation(x, y);
  }
}

void EditorView::refresh()
{
  if (!this->game || this->shownRevision == this->game->revision()) return;
  this->shownRevision = this->game->revision();

  const sgf::Node& root     = this->game->root();
  const sgf::Node& node     = this->game->current();
  const Position&  position = this->game->position();

  this->blackName->setText(Player(root, "PB", "BR", "Black"));
  this->whiteName->setText(Player(root, "PW", "WR", "White"));
  this->blackCaptures->setText("captures " + std::to_string(position.captures(Stone::Black)));
  this->whiteCaptures->setText("captures " + std::to_string(position.captures(Stone::White)));

  // "Move 45  Q16", whose turn, and what hangs off this point.
  std::string move = "Move " + std::to_string(this->game->moveNumber());
  if (const Stone moved = Game::MoveColor(node); moved != Stone::None) {
    const Point p = this->game->movePoint(node);
    move += p.valid() ? "  " + DisplayName(p, this->game->width(), this->game->height()) : std::string("  pass");
  }
  this->moveLabel->setText(move);
  this->turnLabel->setText(std::string(this->game->toPlay() == Stone::Black ? "Black" : "White") + " to play");
  this->variationsLabel->setText(node.childCount() > 1 ? std::to_string(node.childCount()) + " variations from here"
                                 : node.childCount() == 0 ? std::string("end of the line")
                                                          : std::string());

  std::string about;
  const auto add = [&about](const std::string& part) { about += (about.empty() ? "" : "  ") + part; };
  if (!root.get("KM").empty()) add("komi " + root.get("KM"));
  if (!root.get("HA").empty()) add("H" + root.get("HA"));
  if (!root.get("RE").empty()) add(root.get("RE"));
  this->gameLabel->setText(about);

  // The comment: only when another node's comes up, or it changed some
  // other way -- an undo -- while the box wasn't being typed in, or the
  // caret would jump about under the typing.
  const std::string& text = node.get("C");
  if (this->commentNode != &node || (!this->comment->isFocused() && this->comment->getText() != text)) {
    this->comment->setText(text);
    this->commentNode = &node;
  }
  if (!this->nodeName->isFocused() && this->nodeName->getText() != node.get("N")) this->nodeName->setText(node.get("N"));

  const std::string positionId = this->game->positionAnnotation();
  const std::string moveId     = this->game->moveAnnotation();
  for (int i = 0; i < int(std::size(POSITION_NOTES)); ++i) {
    if (positionId == POSITION_NOTES[i][1] && this->positionNote->getSelectedIndex() != i) this->positionNote->setSelectedIndex(i);
  }
  for (int i = 0; i < int(std::size(MOVE_NOTES)); ++i) {
    if (moveId == MOVE_NOTES[i][1] && this->moveNote->getSelectedIndex() != i) this->moveNote->setSelectedIndex(i);
  }
}

void EditorView::layout(int screenWidth, int screenHeight)
{
  // The bar across the top, the panel down the right, the board in the rest.
  this->topBar.setLocation(0, 0);
  if (this->topBar.style.getMinimalWidth() != screenWidth) {
    this->topBar.style.setMinimalWidth(screenWidth);
    this->topBar.style.setMaximalWidth(screenWidth);
  }
  const int top   = this->topBar.getHeight();
  const int sideW = std::min(SIDE_W, screenWidth / 2);
  const int sideH = std::max(0, screenHeight - top);
  this->side.setLocation(screenWidth - sideW, top);
  if (this->side.style.getMinimalWidth() != sideW || this->side.style.getMinimalHeight() != sideH) {
    Pin(this->side, this->side.style, sideW, sideH);
  }

  // The comment and the tree share what the rest of the panel leaves,
  // measured off the last layout rather than guessed at.
  const int inner  = sideW - this->side.getHorizontalPaddings();
  const int chrome = this->side.getVerticalPaddings() + this->upper->getHeight() + this->status->getHeight() + 3 * 4 + 8;
  const int room   = std::max(MIN_COMMENT_H + MIN_TREE_H, sideH - chrome);
  const int commentH = std::max(MIN_COMMENT_H, room * 2 / 5);
  const int treeH    = std::max(MIN_TREE_H, room - commentH);
  if (this->comment->getWidth() != inner || this->comment->getHeight() != commentH) {
    Pin(*this->comment, this->comment->style, inner, commentH);
  }
  this->tree.setSize(inner, treeH);
  PinWidth(this->status->style, inner);

  this->board.layout(BOARD_GAP, top + BOARD_GAP, screenWidth - sideW - 2 * BOARD_GAP, screenHeight - top - 2 * BOARD_GAP);
}

}  // namespace ui
