#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Settings.h"

namespace smoke {
inline bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return uint32_t(now - since) >= interval;
}

enum class EventKind : uint8_t { AlarmStart, AlarmClear, TestFailed };
struct Event {
  EventKind kind;
  uint64_t atMs;
  bool occurredOffline;
};

// Fixed memory, FIFO order, with lower-priority records evicted for alarm starts.
class EventQueue {
 public:
  void push(Event event, bool front = false);
  const Event* front() const { return count_ ? &items_[0] : nullptr; }
  void pop();
  size_t size() const { return count_; }
  uint32_t dropped() const { return dropped_; }
  void acknowledgeDropped(uint32_t count) { dropped_ -= count; }
 private:
  void remove(size_t index);
  Event items_[kEventCapacity]{};
  size_t count_ = 0;
  uint32_t dropped_ = 0;
};

enum class TestState : uint8_t { Idle, Waiting, Detected, Failed };
enum class SilenceState : uint8_t {
  Idle, Confirm, Requested, Cleared, StillDetected, Expired
};

class Detector {
 public:
  void begin(uint32_t now);
  void tick(uint32_t now);
  void sampleAlarm(uint16_t value, uint32_t now);
  void sampleMotion(bool high, uint32_t now, uint32_t epoch);
  void setConnected(bool connected, uint32_t now);
  bool requestTest(uint32_t now);
  bool prepareSilence(uint32_t now);
  bool confirmSilence(uint32_t now);
  bool beginOta(uint32_t now);
  void endOta() { updating_ = false; }
  bool canOta() const { return !alarm_ && !testSession_ && !buttonPressed_ && !updating_; }
  bool alarmActive() const { return alarm_; }
  bool buttonPressed() const { return buttonPressed_; }
  bool testRunning() const { return testSession_; }
  bool connected() const { return connected_; }
  uint64_t uptimeMs() const { return uptimeMs_; }
  uint32_t lastMotionEpoch() const { return lastMotionEpoch_; }
  uint32_t lastTestEpoch() const { return lastTestEpoch_; }
  void recordTestEpoch(uint32_t epoch) { lastTestEpoch_ = epoch; }
  uint32_t testSuccessSequence() const { return testSuccessSequence_; }
  uint32_t revision() const { return revision_; }
  TestState testState() const { return testState_; }
  SilenceState silenceState() const { return silenceState_; }
  const char* testText() const;
  const char* silenceText() const;
  bool motionReady() const { return motionReady_; }
  uint32_t motionTotal() const { return publishedMotion_; }
  void acknowledgeMotion() { motionReady_ = false; }
  bool healthReady() const { return healthReady_; }
  void acknowledgeHealth() { healthReady_ = false; }
  uint32_t offlineMotion() const { return offlineMotion_; }
  void acknowledgeOfflineMotion(uint32_t count) { offlineMotion_ -= count; }
  EventQueue events;
 private:
  void clearAlarm();
  void emit(EventKind kind);
  void change() { ++revision_; }
  uint32_t bootAt_ = 0, lastTick_ = 0, bucketAt_ = 0, healthAt_ = 0;
  uint64_t uptimeMs_ = 0;
  uint32_t clearAt_ = 0, pressAt_ = 0, pressDuration_ = 0;
  uint32_t testAt_ = 0, confirmAt_ = 0, silenceAt_ = 0;
  uint32_t bucketMotion_ = 0, publishedMotion_ = 0, offlineMotion_ = 0;
  uint32_t lastMotionEpoch_ = 0, lastTestEpoch_ = 0;
  uint32_t testSuccessSequence_ = 0, revision_ = 0;
  bool connected_ = false, alarm_ = false, clearing_ = false;
  bool previousPir_ = false, pirReady_ = false, bucketComplete_ = false;
  bool buttonPressed_ = false, updating_ = false, testSession_ = false;
  bool alarmNotified_ = false, everSilenced_ = false, silenceWaiting_ = false;
  bool motionReady_ = false, healthReady_ = false;
  TestState testState_ = TestState::Idle;
  SilenceState silenceState_ = SilenceState::Idle;
};

class Backoff {
 public:
  bool ready(uint32_t now) const { return !attempted_ || elapsed(now, last_, wait_); }
  void attempted(uint32_t now) {
    last_ = now;
    wait_ = next_;
    next_ = next_ < kReconnectMaxMs / 2 ? next_ * 2 : kReconnectMaxMs;
    attempted_ = true;
  }
  void reset() { attempted_ = false; next_ = kReconnectMinMs; }
 private:
  uint32_t last_ = 0, wait_ = 0, next_ = kReconnectMinMs;
  bool attempted_ = false;
};

// Sliding 24-hour window, so reconnects and calendar changes do not reset limits.
class EventLimiter {
 public:
  bool ready(uint64_t now) {
    while (count_ && now - sent_[head_] >= 86400000ULL) {
      head_ = (head_ + 1) % kEventDailyLimit;
      --count_;
    }
    return count_ < kEventDailyLimit && (!ever_ || now - last_ >= kEventSpacingMs);
  }
  void record(uint64_t now) {
    sent_[(head_ + count_) % kEventDailyLimit] = now;
    ++count_;
    ever_ = true;
    last_ = now;
  }
 private:
  uint64_t sent_[kEventDailyLimit]{}, last_ = 0;
  uint8_t head_ = 0, count_ = 0;
  bool ever_ = false;
};
}  // namespace smoke
