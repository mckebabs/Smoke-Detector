#pragma once
#include "Detector.h"

namespace smoke {
// Timers and physical activation are independent of Wi-Fi/cloud operations.
class UpdateMode {
 public:
  bool request(uint32_t now, bool allowed) {
    if (!allowed || pending_ || active_) return false;
    pending_ = true;
    since_ = now;
    return true;
  }
  bool ready(uint32_t now) const {
    return pending_ && elapsed(now, since_, kUpdateStartDelayMs);
  }
  void started(uint32_t now) { pending_ = false; active_ = true; since_ = now; }
  void stop() { pending_ = active_ = false; }
  bool expired(uint32_t now) const {
    return active_ && elapsed(now, since_, kUpdateWindowMs);
  }
  bool pending() const { return pending_; }
  bool active() const { return active_; }
  bool busy() const { return pending_ || active_; }
  bool sampleButton(bool pressed, uint32_t now) {
    if (!pressed) { armed_ = true; holding_ = fired_ = false; return false; }
    if (!armed_) return false; // Require a release after startup.
    if (!holding_) { holding_ = true; heldAt_ = now; }
    if (fired_ || !elapsed(now, heldAt_, kUpdateHoldMs)) return false;
    fired_ = true;
    return true;
  }
 private:
  uint32_t since_ = 0, heldAt_ = 0;
  bool pending_ = false, active_ = false;
  bool armed_ = false, holding_ = false, fired_ = false;
};
} // namespace smoke
