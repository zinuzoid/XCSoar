// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Tracking/JETProvider/TrafficDecode.hpp"
#include "TestUtil.hpp"

int main()
{
  plan_tests(16);

  /* valid: 6-digit uppercase hex */
  FlarmId id = JETProvider::ParseTrafficId("DDA5BA");
  ok1(id.IsDefined());

  /* valid: lowercase equals the uppercase value */
  FlarmId id_lower = JETProvider::ParseTrafficId("dda5ba");
  ok1(id_lower.IsDefined());
  ok1(id_lower == id);

  /* valid: maximum 32-bit value */
  ok1(JETProvider::ParseTrafficId("FFFFFFFF").IsDefined());

  /* invalid: nullptr */
  ok1(!JETProvider::ParseTrafficId(nullptr).IsDefined());

  /* invalid: empty string */
  ok1(!JETProvider::ParseTrafficId("").IsDefined());

  /* invalid: zero is FlarmId::Undefined() */
  ok1(!JETProvider::ParseTrafficId("0").IsDefined());
  ok1(!JETProvider::ParseTrafficId("000000").IsDefined());

  /* invalid: leading whitespace (strtol would accept) */
  ok1(!JETProvider::ParseTrafficId(" DDA5BA").IsDefined());

  /* invalid: negative (strtol would accept) */
  ok1(!JETProvider::ParseTrafficId("-1").IsDefined());

  /* invalid: 0x prefix (strtol would accept, causing collisions) */
  ok1(!JETProvider::ParseTrafficId("0xDDA5BA").IsDefined());

  /* invalid: OGN prefix — non-hex chars degrade to no-dedup */
  ok1(!JETProvider::ParseTrafficId("FLRDDA5BA").IsDefined());

  /* invalid: non-hex characters */
  ok1(!JETProvider::ParseTrafficId("zz").IsDefined());

  /* invalid: 9 hex digits (exceeds 8-char limit) */
  ok1(!JETProvider::ParseTrafficId("FFFFFFFFF").IsDefined());

  /* invalid: 16 hex digits (would overflow uint32_t; strtol saturates) */
  ok1(!JETProvider::ParseTrafficId("FFFFFFFFFFFFFFFF").IsDefined());

  return exit_status();
}
