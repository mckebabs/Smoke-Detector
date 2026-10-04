#include "Settings.h"
#include "TimeDisplay.h"
#include <stdlib.h>
#include <string.h>

using namespace smoke;
static int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static void readable_time_and_daylight_saving() {
  CHECK(setenv("TZ", kTimeZone, 1) == 0);
  tzset();
  char display[32];
  struct Example { uint32_t epoch; const char* expected; };
  const Example examples[] = {
    {1791148020, "2026-10-05 00:07"},
    {1767225600, "2026-01-01 02:00"},
    {1782864000, "2026-07-01 03:00"},
    {1774745940, "2026-03-29 02:59"},
    {1774746000, "2026-03-29 04:00"},
    {1792889940, "2026-10-25 03:59"},
    {1792890000, "2026-10-25 03:00"},
    {0, "Not recorded since restart"}
  };
  for (const auto& example : examples) {
    formatLocalTime(example.epoch, display, sizeof(display));
    CHECK(strcmp(display, example.expected) == 0);
  }
}

int main() {
  readable_time_and_daylight_saving();
  printf("PASS: 1 date/time scenario group, %d assertions.\n", assertions);
}
