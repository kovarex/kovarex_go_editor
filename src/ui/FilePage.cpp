#include <ui/FilePage.hpp>

#include <app/Platform.hpp>
#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/ListBox.hpp>
#include <Agui/Widget/TextField.hpp>

#include <algorithm>
#include <cwctype>
#include <system_error>

namespace ui {

namespace {

constexpr int LIST_W = 560;
constexpr int LIST_H = 340;

bool IsSgf(const std::filesystem::path& path)
{
  std::wstring ext = path.extension().wstring();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
  return ext == L".sgf";
}

// Case doesn't matter to Windows, so it doesn't to the order either.
bool Before(const std::filesystem::path& a, const std::filesystem::path& b)
{
  std::wstring x = a.filename().wstring(), y = b.filename().wstring();
  std::transform(x.begin(), x.end(), x.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
  std::transform(y.begin(), y.end(), y.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
  return x < y;
}

}  // namespace

FilePage::FilePage(Theme& theme, std::function<void(const std::filesystem::path&)> onChosen, std::function<void()> onBack)
    : theme(theme)
    , window(agui::GuiDirection::Vertical, "Open")
    , onChosen(std::move(onChosen))
{
  this->window.setDragTarget(&this->window);

  agui::VerticalFlow& content = column();

  agui::HorizontalFlow& top = row(8);
  this->drive = &make<agui::DropDown>();
  this->drive->style.setMinimalWidth(80);
  this->drive->style.setMaximalWidth(80);
  this->drive->onItemSelect(this, [this](int i) {
    if (i >= 0 && size_t(i) < this->drives.size()) this->enter(this->drives[size_t(i)]);
  });
  top << this->drive;
  top << agui::button("Up", &this->window, [this] {
    if (this->current.has_parent_path() && this->current.parent_path() != this->current) this->enter(this->current.parent_path());
  }, &theme.smallButton);
  this->where = &agui::label("", &theme.dimLabel);
  top << *this->where;
  content << top;

  this->list = &make<agui::ListBox>();
  this->list->style.setMinimalWidth(LIST_W);
  this->list->style.setMaximalWidth(LIST_W);
  this->list->style.setMinimalHeight(LIST_H);
  this->list->style.setMaximalHeight(LIST_H);
  this->list->onItemSelect(this, [this](int i) { this->picked(i); });
  this->list->onItemDoubleClick(this, [this](int i) { this->opened(i); });
  content << *this->list;

  this->name = &make<agui::TextField>();
  this->name->style.setMinimalWidth(LIST_W - 160);
  this->name->style.setMaximalWidth(LIST_W - 160);
  this->name->onConfirm(this, [this] { this->confirm(); });
  content << namedRow("File name", *this->name, 150);

  this->problem = &agui::label("", &theme.badLabel);
  content << *this->problem;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << content;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  this->confirmButton = &footerButton("Open", &this->window, [this] { this->confirm(); }, &theme.forwardButton, 160);
  footer << *this->confirmButton;
  this->window << footer;
}

void FilePage::start(Mode newMode, const std::filesystem::path& folder, const std::string& fileName)
{
  this->mode = newMode;
  this->window.title.setText(std::string(newMode == Mode::Open ? "Open" : "Save as"));
  this->confirmButton->setText(std::string(newMode == Mode::Open ? "Open" : "Save"));
  this->name->setText(fileName);
  this->problem->setText(std::string());

  this->drives = platform::Drives();
  this->drive->clearItems();
  for (const std::filesystem::path& d : this->drives) this->drive->addItem(platform::ToUtf8(d));

  std::error_code error;
  this->enter(std::filesystem::is_directory(folder, error) ? folder : platform::DocumentsFolder());
}

void FilePage::enter(const std::filesystem::path& folder)
{
  std::error_code error;
  std::vector<Entry> found;
  for (std::filesystem::directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, error), end;
       !error && it != end; it.increment(error)) {
    std::error_code ignored;
    const bool isFolder = it->is_directory(ignored);
    if (isFolder || IsSgf(it->path())) found.push_back({ it->path(), isFolder });
  }
  if (error) {
    this->problem->setText("Can't look in " + platform::ToUtf8(folder) + ".");
    return;
  }

  // Folders first, then files, each in name order.
  std::sort(found.begin(), found.end(), [](const Entry& a, const Entry& b) {
    if (a.folder != b.folder) return a.folder;
    return Before(a.path, b.path);
  });

  this->current = folder;
  this->entries.clear();
  if (folder.has_parent_path() && folder.parent_path() != folder) this->entries.push_back({ folder.parent_path(), true });
  for (Entry& e : found) this->entries.push_back(std::move(e));

  this->list->clearItems();
  for (size_t i = 0; i < this->entries.size(); ++i) {
    const Entry& e = this->entries[i];
    const bool   up = i == 0 && e.path == folder.parent_path();
    this->list->addItem(up ? std::string("..") : e.folder ? "[" + platform::ToUtf8(e.path.filename()) + "]"
                                                          : platform::ToUtf8(e.path.filename()));
  }
  this->where->setText(platform::ToUtf8(folder));
  this->problem->setText(std::string());

  const std::wstring root = folder.root_path().wstring();
  for (size_t i = 0; i < this->drives.size(); ++i) {
    if (_wcsicmp(this->drives[i].wstring().c_str(), root.c_str()) == 0) this->drive->setSelectedIndex(int(i));
  }
}

void FilePage::picked(int index)
{
  if (index < 0 || size_t(index) >= this->entries.size()) return;
  const Entry& e = this->entries[size_t(index)];
  if (!e.folder) this->name->setText(platform::ToUtf8(e.path.filename()));
}

void FilePage::opened(int index)
{
  if (index < 0 || size_t(index) >= this->entries.size()) return;
  const Entry e = this->entries[size_t(index)];
  if (e.folder) this->enter(e.path);
  else          this->onChosen(e.path);
}

void FilePage::confirm()
{
  const std::string typed = this->name->getText();
  if (typed.empty()) {
    this->problem->setText(std::string(this->mode == Mode::Open ? "Pick a file to open." : "Type a name to save as."));
    return;
  }

  std::filesystem::path path = platform::FromUtf8(typed);
  if (path.is_relative()) path = this->current / path;

  std::error_code error;
  if (std::filesystem::is_directory(path, error)) {
    this->enter(path);
    this->name->setText(std::string());
    return;
  }

  if (this->mode == Mode::Open) {
    if (!std::filesystem::exists(path, error)) {
      this->problem->setText("There is no " + platform::ToUtf8(path.filename()) + " here.");
      return;
    }
  } else if (!IsSgf(path)) {
    path += L".sgf";
  }
  this->onChosen(path);
}

}  // namespace ui
