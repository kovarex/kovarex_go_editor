#include <ui/AboutPage.hpp>

#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Table.hpp>

namespace ui {

namespace {

// The icon, as big as Factorio's About shows its own.
constexpr int ICON_PX = 128;
// As wide as the story is allowed to run before it wraps.
constexpr int TEXT_W = 440;

constexpr const char* STORY =
    "This is my first attempt at vibe coding. I'm quite impressed. This was written without writing a single line of "
    "code manually. It had access to Factorio resources and source code to replicate its UI.";

}  // namespace

AboutPage::AboutPage(Theme& theme, std::function<void()> onLink, std::function<void()> onBack)
    : window(agui::GuiDirection::Vertical, "About")
{
  this->window.setDragTarget(&this->window);

  // As Factorio's AboutGui: the picture, and beside it a panel with a table of
  // what is what.
  agui::ImageWidget& icon = make<agui::ImageWidget>(theme.appIcon());
  icon.scaleToKeepTheRatio = true;
  icon.style.setMinimalWidth(ICON_PX);
  icon.style.setMaximalWidth(ICON_PX);
  icon.style.setMinimalHeight(ICON_PX);
  icon.style.setMaximalHeight(ICON_PX);

  agui::Table& facts = make<agui::Table>(2u);
  facts << agui::label("Author:") << agui::label("kovarex");
  // HyperLink: a label in hyperlink_label that opens the address.
  agui::Label& link = agui::label(PROJECT_URL, &theme.linkLabel);
  link.onClick(this, [onLink = std::move(onLink)] { onLink(); });
  facts << agui::label("Hosted on:") << link;

  agui::Label& story = agui::label(STORY, agui::SingleLine::False);
  story.style.setMaximalWidth(TEXT_W);
  story.style.setMinimalWidth(TEXT_W);

  agui::VerticalFlow& text = column(12);
  text << facts << story;
  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << text;

  agui::HorizontalFlow& body = row(12);
  body.style.setVerticalAlign(agui::VerticalAlign::Top);
  body << icon << panel;
  this->window << body;

  // Dialog buttons, Factorio's only one being Confirm: nothing here to keep
  // or throw away, so it and Esc just close the page.
  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Confirm", &this->window, std::move(onBack), &theme.forwardButton, 200);
  this->window << footer;
}

}  // namespace ui
