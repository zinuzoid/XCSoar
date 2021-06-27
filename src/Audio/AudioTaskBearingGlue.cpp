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

#include "AudioTaskBearingGlue.hpp"
#include "PCMPlayer.hpp"
#include "PCMPlayerFactory.hpp"
#include "VarioSynthesiser.hpp"
#include "VarioSettings.hpp"
#include "Math/Util.hpp"

#ifdef ANDROID
#include "SLES/Init.hpp"
#endif

#include <cassert>

static constexpr unsigned sample_rate = 44100;

#ifdef ANDROID
static bool have_sles;
#endif

static PCMPlayer *player;
static VarioSynthesiser *synthesiser;

bool
AudioTaskBearingGlue::HaveAudioVario()
{
#ifdef ANDROID
  return have_sles;
#else
  return true;
#endif
}

void
AudioTaskBearingGlue::Initialise()
{
  assert(player == nullptr);
  assert(synthesiser == nullptr);

#ifdef ANDROID
  have_sles = SLES::Initialise();
  if (!have_sles)
    return;
#endif

  player = PCMPlayerFactory::CreateInstance();
  synthesiser = new VarioSynthesiser(sample_rate);
}

void
AudioTaskBearingGlue::Deinitialise()
{
  delete player;
  player = nullptr;
  delete synthesiser;
  synthesiser = nullptr;
}

void
AudioTaskBearingGlue::Configure(const VarioSoundSettings &settings)
{
#ifdef ANDROID
  if (!have_sles)
    return;
#endif

  assert(player != nullptr);
  assert(synthesiser != nullptr);

  if (settings.enabled) {
    synthesiser->SetVolume(settings.volume);
    synthesiser->SetDeadBand(settings.dead_band_enabled);
    synthesiser->SetFrequencies(settings.min_frequency, settings.zero_frequency,
                                settings.max_frequency);
    synthesiser->SetPeriods(settings.min_period_ms, settings.max_period_ms);
    synthesiser->SetDeadBandRange(settings.min_dead, settings.max_dead);
    synthesiser->SetMinMaxVario(-90 * 100, 90 * 100);
    player->Start(*synthesiser);
  } else
    player->Stop();
}

void
AudioTaskBearingGlue::SetValue(double vario)
{
#ifdef ANDROID
  if (!have_sles)
    return;
#endif

  assert(player != nullptr);
  assert(synthesiser != nullptr);

  synthesiser->SetVario(fabs(vario));
}

void
AudioTaskBearingGlue::NoValue()
{
#ifdef ANDROID
  if (!have_sles)
    return;
#endif

  assert(player != nullptr);
  assert(synthesiser != nullptr);

  synthesiser->SetSilence();
}
