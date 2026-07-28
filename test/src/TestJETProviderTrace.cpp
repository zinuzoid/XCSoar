// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Tracking/JETProvider/TraceParser.hpp"
#include "TestUtil.hpp"

using namespace JETProvider;

/* the canonical example from Google's polyline documentation:
   (38.5, -120.2), (40.7, -120.95), (43.252, -126.453) */
static constexpr char DEMO[] = "_p~iF~ps|U_ulLnnqC_mqNvxq`@";

int main()
{
  plan_tests(47);

  /* JoinPilotIds() */
  ok1(JoinPilotIds("").empty());
  ok1(JoinPilotIds("  ").empty());
  ok1(JoinPilotIds(",,").empty());
  ok1(JoinPilotIds("6c79fb24") == "6c79fb24");
  ok1(JoinPilotIds("6c79fb24,99f95de4") == "6c79fb24,99f95de4");
  ok1(JoinPilotIds(" 6c79fb24 , 99f95de4 ") == "6c79fb24,99f95de4");
  ok1(JoinPilotIds("6c79fb24,,99f95de4,") == "6c79fb24,99f95de4");

  /* two pilots, two lines each */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n"};
    body += DEMO;
    body += "\nBob\n";
    body += DEMO;
    body += "\n";

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 2);
    ok1(traces.count("Alice") == 1);
    ok1(traces.count("Bob") == 1);
    ok1(traces["Alice"].id == "Alice");
    ok1(traces["Alice"].points.size() == 3);
    ok1(equals(traces["Bob"].points[0].latitude.Degrees(), 38.5));
    ok1(equals(traces["Bob"].points[0].longitude.Degrees(), -120.2));
    ok1(equals(traces["Bob"].points[2].latitude.Degrees(), 43.252));
  }

  /* no trailing newline on the last line */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n"};
    body += DEMO;

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces["Alice"].points.size() == 3);
  }

  /* CRLF line endings */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\r\n"};
    body += DEMO;
    body += "\r\n";

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces["Alice"].points.size() == 3);
  }

  /* a pilot with no trace yet: empty polyline line, and the records
     after it must still line up */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n\nBob\n"};
    body += DEMO;
    body += "\n";

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces.count("Alice") == 0);
    ok1(traces.count("Bob") == 1);
    ok1(traces["Bob"].points.size() == 3);
  }

  /* empty body */
  {
    std::map<std::string, PilotTrace> traces;
    ok1(ParseTraceResponse("", traces));
    ok1(traces.empty());
    ok1(ParseTraceResponse("\n\n", traces));
    ok1(traces.empty());
  }

  /* the last pilot has no trace and the body ends right after the name,
     with no empty line for it - still a success */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n"};
    body += DEMO;
    body += "\nBob\n";

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces.count("Alice") == 1);
    ok1(traces.count("Bob") == 0);
  }

  /* ... and the same without the final newline either */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n"};
    body += DEMO;
    body += "\nBob";

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces.count("Alice") == 1);
  }

  /* a "top N" response: several pilots have no trace yet, including the
     last one - all of that is a success, not a malformed response */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body;
    for (unsigned i = 1; i <= 10; ++i) {
      body += "pilot";
      body += (char)('0' + i % 10);
      body += "\n";
      /* only the odd ones are flying */
      if (i % 2)
        body += DEMO;
      body += "\n";
    }

    ok1(ParseTraceResponse(body, traces));
    ok1(traces.size() == 5);
    ok1(traces.count("pilot1") == 1);
    ok1(traces.count("pilot2") == 0);
    ok1(traces.count("pilot9") == 1);
    ok1(traces.count("pilot0") == 0);
  }

  /* a malformed polyline must not discard the other pilots */
  {
    std::map<std::string, PilotTrace> traces;
    std::string body{"Alice\n!!!not a polyline!!!\nBob\n"};
    body += DEMO;
    body += "\n";

    ok1(!ParseTraceResponse(body, traces));
    ok1(traces.size() == 1);
    ok1(traces.count("Bob") == 1);
  }

  return exit_status();
}
