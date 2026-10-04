#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

namespace smoke {
// The caller configures Europe/Riga before using this helper. Epochs stay UTC.
inline void formatLocalTime(uint32_t epoch, char* output, size_t size) {
  const time_t value = static_cast<time_t>(epoch);
  struct tm local {};
  if (epoch >= 1704067200 && localtime_r(&value, &local) &&
      strftime(output, size, "%Y-%m-%d %H:%M", &local)) return;
  snprintf(output, size, "Not recorded since restart");
}
}  // namespace smoke
