/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2021 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "AudioTaskBearingConfigPanel.hpp"
#include "Profile/Keys.hpp"
#include "Language/Language.hpp"
#include "Interface.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Float.hpp"
#include "UIGlobals.hpp"
#include "Audio/Features.hpp"
#include "Audio/AudioTaskBearingGlue.hpp"
#include "Units/Units.hpp"
#include "Formatter/UserUnits.hpp"

enum ControlIndex {
  Enabled,
  Volume,
  DEAD_BAND_ENABLED,
  SPACER,
  MIN_FREQUENCY,
  ZERO_FREQUENCY,
  MAX_FREQUENCY,
  SPACER2,
  DEAD_BAND_MIN,
  DEAD_BAND_MAX,
};


class AudioTaskBearingConfigPanel final : public RowFormWidget {
public:
  AudioTaskBearingConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
AudioTaskBearingConfigPanel::Prepare(ContainerWindow &parent,
                               const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  if (!AudioTaskBearingGlue::HaveAudioVario())
    return;

  const auto &settings = CommonInterface::GetUISettings().sound.task_bearing;

  AddBoolean(_("Enable"),
             nullptr,
             settings.enabled);

  AddInteger(_("Volume"), nullptr, _T("%u %%"), _T("%u"),
             0, 100, 1, settings.volume);

  AddBoolean(_("Enable Deadband"),
             nullptr, settings.dead_band_enabled);

  AddSpacer();
  SetExpertRow(SPACER);

  AddInteger(_("Min. Frequency"),
             _("The tone frequency that is played at maximum sink rate."),
             _T("%u Hz"), _T("%u"),
             50, 3000, 50, settings.min_frequency);
  SetExpertRow(MIN_FREQUENCY);

  AddInteger(_("Zero Frequency"),
             _("The tone frequency that is played at zero climb rate."),
             _T("%u Hz"), _T("%u"),
             50, 3000, 50, settings.zero_frequency);
  SetExpertRow(ZERO_FREQUENCY);

  AddInteger(_("Max. Frequency"),
             _("The tone frequency that is played at maximum climb rate."),
             _T("%u Hz"), _T("%u"),
             50, 3000, 50, settings.max_frequency);
  SetExpertRow(MAX_FREQUENCY);

  AddSpacer();
  SetExpertRow(SPACER2);

  AddFloat(_("Deadband min."),
           nullptr,
           _T("%.1f°"), _T("%.1f°"),
           -45.0, 0,
           0.5, false,
           settings.min_dead);
  SetExpertRow(DEAD_BAND_MIN);
  DataFieldFloat &db_min = (DataFieldFloat &)GetDataField(DEAD_BAND_MIN);
  db_min.SetFormat(GetUserVerticalSpeedFormat(false, true));

  AddFloat(_("Deadband max."),
           nullptr,
           _T("%.1f°"), _T("%.1f°"),
           0, 45.0,
           0.5, false,
           settings.max_dead);
  SetExpertRow(DEAD_BAND_MAX);
  DataFieldFloat &db_max = (DataFieldFloat &)GetDataField(DEAD_BAND_MAX);
  db_max.SetFormat(GetUserVerticalSpeedFormat(false, true));
}

bool
AudioTaskBearingConfigPanel::Save(bool &changed) noexcept
{
  if (!AudioTaskBearingGlue::HaveAudioVario())
    return true;

  auto &settings = CommonInterface::SetUISettings().sound.task_bearing;

  changed |= SaveValue(Enabled, ProfileKeys::AudioTaskBearingEnable,
                       settings.enabled);

  unsigned volume = settings.volume;
  if (SaveValueInteger(Volume, ProfileKeys::AudioTaskBearingVolume, volume)) {
    settings.volume = volume;
    changed = true;
  }

  changed |= SaveValue(DEAD_BAND_ENABLED, ProfileKeys::AudioTaskBearingDeadBandEnabled,
                       settings.dead_band_enabled);

  changed |= SaveValueInteger(MIN_FREQUENCY, ProfileKeys::AudioTaskBearingMinFrequency,
                       settings.min_frequency);

  changed |= SaveValueInteger(ZERO_FREQUENCY, ProfileKeys::AudioTaskBearingZeroFrequency,
                       settings.zero_frequency);

  changed |= SaveValueInteger(MAX_FREQUENCY, ProfileKeys::AudioTaskBearingMaxFrequency,
                       settings.max_frequency);

  changed |= SaveValue(DEAD_BAND_MIN, UnitGroup::NONE,
                       ProfileKeys::AudioTaskBearingDeadBandMin, settings.min_dead);

  changed |= SaveValue(DEAD_BAND_MAX, UnitGroup::NONE,
                       ProfileKeys::AudioTaskBearingDeadBandMax, settings.max_dead);

  return true;
}

std::unique_ptr<Widget>
CreateAudioTaskBearingConfigPanel()
{
  return std::make_unique<AudioTaskBearingConfigPanel>();
}
