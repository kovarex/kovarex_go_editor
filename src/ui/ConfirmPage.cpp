#include <ui/ConfirmPage.hpp>

#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/Label.hpp>

namespace ui {

ConfirmPage::ConfirmPage(Theme& theme, std::function<void()> onSave, std::function<void()> onDiscard,
                         std::function<void()> onBack)
    : window(agui::GuiDirection::Vertical, "Unsaved changes")
{
  this->window.setDragTarget(&this->window);

  this->question = &agui::label("");
  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << *this->question;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Cancel", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Don't save", &this->window, std::move(onDiscard), &theme.redBackButton, 140);
  footer << footerButton("Save", &this->window, std::move(onSave), &theme.forwardButton, 140);
  this->window << footer;
}

void ConfirmPage::setFile(const std::string& name)
{
  this->question->setText("Save the changes to " + name + " first?");
}

}  // namespace ui
