// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SkysightDialog.hpp"
#include "Weather/Features.hpp"

#ifdef HAVE_SKYSIGHT

#include "DataGlobals.hpp"
#include "Dialogs/ListPicker.hpp"
#include "Dialogs/Message.hpp"
#include "Form/Button.hpp"
#include "Form/ButtonPanel.hpp"
#include "Formatter/LocalTimeFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "UIGlobals.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Widget/ButtonPanelWidget.hpp"
#include "Widget/ListWidget.hpp"
#include "time/BrokenDateTime.hpp"
#include "time/Stamp.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "util/StaticString.hxx"

#include <memory>
#include <string>

/**
 * Renderer for the "Add layer" picker list.
 */
class SkysightLayerPickerRenderer final : public ListItemRenderer {
  TextRowRenderer row_renderer;
  const Skysight &skysight;

public:
  explicit SkysightLayerPickerRenderer(const Skysight &_skysight)
    :skysight(_skysight) {}

  unsigned CalculateLayout(const DialogLook &look) {
    return row_renderer.CalculateLayout(*look.list.font);
  }

  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned i) noexcept override {
    const auto &layers = skysight.GetLayers();
    if (i < layers.size())
      row_renderer.DrawTextRow(canvas, rc, layers[i].name.c_str());
  }

  static const TCHAR *HelpCallback(unsigned i) {
    const auto skysight = DataGlobals::GetSkysight();
    if (!skysight || i >= skysight->GetLayers().size())
      return _("No description available.");

    /* UI thread only; keeps the string alive while the help dialog
       is open */
    static std::string help;
    help = skysight->GetLayers()[i].description;
    return help.empty()
      ? _("No description available.")
      : help.c_str();
  }
};

class SkysightWidget final
  : public ListWidget, SkysightListener
{
  ButtonPanelWidget *buttons_widget = nullptr;

  Button *activate_button, *deactivate_button, *add_button,
    *remove_button, *update_button, *updateall_button;

  TwoTextRowsRenderer row_renderer;

  const std::shared_ptr<Skysight> skysight;

  /* only for refreshing the relative "updated ... ago" texts; state
     changes arrive through OnSkysightUpdate() */
  UI::PeriodicTimer refresh_timer{[this]{ UpdateList(); }};

public:
  explicit SkysightWidget(std::shared_ptr<Skysight> &&_skysight)
    :skysight(std::move(_skysight)) {}

  void SetButtonPanel(ButtonPanelWidget &_buttons) {
    buttons_widget = &_buttons;
  }

private:
  void CreateButtons(ButtonPanel &buttons);

  void UpdateList();
  void UpdateButtons();
  void ActivateClicked();
  void DeactivateClicked();
  void AddClicked();
  void UpdateClicked();
  void UpdateAllClicked();
  void RemoveClicked();

  [[gnu::pure]]
  const Skysight::SelectedLayer *GetCursorLayer() const;

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  void Unprepare() noexcept override;

protected:
  /* virtual methods from ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override;

  /* virtual methods from ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return false;
  }

  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

private:
  /* virtual methods from SkysightListener */
  void OnSkysightUpdate() noexcept override {
    UpdateList();
  }
};

void
SkysightWidget::CreateButtons(ButtonPanel &buttons)
{
  activate_button = buttons.Add(_("Activate"),
                                [this](){ ActivateClicked(); });
  deactivate_button = buttons.Add(_("Deactivate"),
                                  [this](){ DeactivateClicked(); });
  add_button = buttons.Add(_("Add"), [this](){ AddClicked(); });
  remove_button = buttons.Add(_("Remove"), [this](){ RemoveClicked(); });
  update_button = buttons.Add(_("Update"), [this](){ UpdateClicked(); });
  updateall_button = buttons.Add(_("Update All"),
                                 [this](){ UpdateAllClicked(); });
}

void
SkysightWidget::Prepare(ContainerWindow &parent,
                        const PixelRect &rc) noexcept
{
  CreateButtons(buttons_widget->GetButtonPanel());

  const DialogLook &look = UIGlobals::GetDialogLook();
  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font));
  UpdateList();

  if (skysight) {
    skysight->SetListener(this);
    refresh_timer.Schedule(std::chrono::seconds(5));
  }
}

void
SkysightWidget::Unprepare() noexcept
{
  refresh_timer.Cancel();
  if (skysight)
    skysight->SetListener(nullptr);
  DeleteWindow();
}

const Skysight::SelectedLayer *
SkysightWidget::GetCursorLayer() const
{
  return skysight
    ? skysight->GetSelectedLayer(GetList().GetCursorIndex())
    : nullptr;
}

void
SkysightWidget::UpdateList()
{
  const std::size_t n = skysight ? skysight->NumSelectedLayers() : 0;

  ListControl &list = GetList();
  /* an extra "status" row is shown while the list is empty */
  list.SetLength(std::max<std::size_t>(n, 1));
  list.Invalidate();

  UpdateButtons();
}

void
SkysightWidget::UpdateButtons()
{
  const std::size_t n = skysight ? skysight->NumSelectedLayers() : 0;

  const auto *cursor = GetCursorLayer();
  const bool item_updating = cursor != nullptr &&
    skysight->IsLayerUpdating(cursor->id);
  const bool item_active = cursor != nullptr &&
    skysight->GetDisplayedLayerId() == cursor->id;

  add_button->SetEnabled(skysight && skysight->IsReady() &&
                         !skysight->SelectedLayersFull());
  remove_button->SetEnabled(cursor != nullptr && !item_updating);
  update_button->SetEnabled(cursor != nullptr && !item_updating);
  updateall_button->SetEnabled(n > 0 && skysight &&
                               !skysight->IsUpdating());
  activate_button->SetEnabled(cursor != nullptr && !item_updating);
  deactivate_button->SetEnabled(item_active);
}

void
SkysightWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                            unsigned index) noexcept
{
  if (!skysight || skysight->NumSelectedLayers() == 0) {
    /* status row */
    row_renderer.DrawFirstRow(canvas, rc, _("No layers selected"));
    row_renderer.DrawSecondRow(canvas, rc,
                               skysight
                               ? skysight->GetStatusText().c_str()
                               : _("Skysight is not available"));
    return;
  }

  const auto *selected = skysight->GetSelectedLayer(index);
  if (selected == nullptr)
    return;

  const auto *layer = skysight->GetLayer(selected->id);

  StaticString<256> first_row;
  first_row = layer != nullptr
    ? layer->name.c_str()
    : selected->id.c_str();
  if (skysight->GetDisplayedLayerId() == selected->id)
    first_row += _T(" [ACTIVE]");

  StaticString<256> second_row;
  if (skysight->IsLayerUpdating(selected->id)) {
    second_row = _("Updating...");
  } else if (selected->from == 0 || selected->to == 0 ||
             selected->mtime == 0) {
    second_row = _("No data. Press \"Update\" to download.");
  } else {
    const auto &settings = CommonInterface::GetComputerSettings();
    const std::time_t elapsed =
      std::chrono::system_clock::to_time_t(
        BrokenDateTime::NowUTC().ToTimePoint()) - selected->mtime;

    second_row.Format(_("Data from %s to %s. Updated %s ago"),
                      FormatLocalTimeHHMM(
                        TimeStamp{std::chrono::duration<double>(selected->from)},
                        settings.utc_offset).c_str(),
                      FormatLocalTimeHHMM(
                        TimeStamp{std::chrono::duration<double>(selected->to)},
                        settings.utc_offset).c_str(),
                      FormatTimespanSmart(std::chrono::seconds(elapsed)).c_str());
  }

  row_renderer.DrawFirstRow(canvas, rc, first_row.c_str());
  row_renderer.DrawSecondRow(canvas, rc, second_row.c_str());
}

void
SkysightWidget::AddClicked()
{
  if (!skysight || !skysight->IsReady()) {
    ShowMessageBox(
      _("Please check your Skysight settings and internet connection."),
      _("Couldn't connect to Skysight"), MB_OK);
    return;
  }

  SkysightLayerPickerRenderer item_renderer{*skysight};

  int i = ListPicker(_("Choose a parameter"),
                     skysight->GetLayers().size(), 0,
                     item_renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                     item_renderer,
                     false, nullptr,
                     &SkysightLayerPickerRenderer::HelpCallback,
                     nullptr);

  if (i < 0 || (std::size_t)i >= skysight->GetLayers().size())
    return;

  skysight->AddSelectedLayer(skysight->GetLayers()[i].id);
  UpdateList();
}

void
SkysightWidget::UpdateClicked()
{
  if (const auto *selected = GetCursorLayer(); selected != nullptr)
    skysight->UpdateSelectedLayer(selected->id);
  UpdateList();
}

void
SkysightWidget::UpdateAllClicked()
{
  if (skysight)
    skysight->UpdateSelectedLayer({});
  UpdateList();
}

void
SkysightWidget::RemoveClicked()
{
  const auto *selected = GetCursorLayer();
  if (selected == nullptr)
    return;

  const auto *layer = skysight->GetLayer(selected->id);

  StaticString<256> prompt;
  prompt.Format(_("Do you want to remove \"%s\"?"),
                layer != nullptr
                ? layer->name.c_str()
                : selected->id.c_str());

  if (ShowMessageBox(prompt, _("Remove"), MB_YESNO) == IDNO)
    return;

  skysight->RemoveSelectedLayer(selected->id);
  UpdateList();
}

inline void
SkysightWidget::ActivateClicked()
{
  const auto *selected = GetCursorLayer();
  if (selected == nullptr)
    return;

  if (!skysight->DisplayLayer(selected->id.c_str()))
    ShowMessageBox(_("No forecast data available for this time yet; a download has been started."),
                   _("Skysight"), MB_OK);
  UpdateList();
}

inline void
SkysightWidget::DeactivateClicked()
{
  if (skysight)
    skysight->DisplayLayer(nullptr);
  UpdateList();
}

std::unique_ptr<Widget>
CreateSkysightWidget()
{
  auto skysight = DataGlobals::GetSkysight();
  auto buttons =
    std::make_unique<ButtonPanelWidget>(std::make_unique<SkysightWidget>(std::move(skysight)),
                                        ButtonPanelWidget::Alignment::BOTTOM);
  ((SkysightWidget &)buttons->GetWidget()).SetButtonPanel(*buttons);
  return buttons;
}

#endif /* HAVE_SKYSIGHT */
