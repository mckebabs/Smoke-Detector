#include "Detector.h"
#include <limits.h>

namespace smoke {
static void increment(uint32_t& value) { if (value != UINT32_MAX) ++value; }

void EventQueue::remove(size_t index) {
  for (size_t i = index + 1; i < count_; ++i) items_[i - 1] = items_[i];
  --count_;
}
void EventQueue::pop() { if (count_) remove(0); }
void EventQueue::push(Event event, bool front) {
  if (count_ == kEventCapacity) {
    if (event.kind != EventKind::AlarmStart) { increment(dropped_); return; }
    size_t victim = 0;
    while (victim < count_ && items_[victim].kind == EventKind::AlarmStart) ++victim;
    remove(victim < count_ ? victim : 0);
    increment(dropped_);
  }
  if (front) {
    for (size_t i = count_; i > 0; --i) items_[i] = items_[i - 1];
    items_[0] = event;
    ++count_;
  } else {
    items_[count_++] = event;
  }
}

void Detector::begin(uint32_t now) {
  *this = Detector();
  bootAt_ = lastTick_ = bucketAt_ = healthAt_ = now;
}
void Detector::emit(EventKind kind) {
  events.push({kind, uptimeMs_, !connected_});
}
void Detector::tick(uint32_t now) {
  uptimeMs_ += uint32_t(now - lastTick_);
  lastTick_ = now;
  if (buttonPressed_ && elapsed(now, pressAt_, pressDuration_)) {
    buttonPressed_ = false;
    change();
  }
  if (testSession_) {
    if (testState_ == TestState::Waiting && elapsed(now, testAt_, kTestResponseMs)) {
      testState_ = TestState::Failed;
      testSession_ = false;
      emit(EventKind::TestFailed);
      change();
    } else if (elapsed(now, testAt_, kTestEscalateMs)) {
      testSession_ = false;
      if (alarm_ && !alarmNotified_) {
        emit(EventKind::AlarmStart);
        alarmNotified_ = true;
      }
      change();
    }
  }
  if (silenceState_ == SilenceState::Confirm && elapsed(now, confirmAt_, kSilenceConfirmMs)) {
    silenceState_ = SilenceState::Expired;
    change();
  }
  if (silenceWaiting_ && elapsed(now, silenceAt_, kSilenceResultMs)) {
    silenceWaiting_ = false;
    silenceState_ = alarm_ ? SilenceState::StillDetected : SilenceState::Cleared;
    change();
  }
  if (elapsed(now, bucketAt_, kMotionBucketMs)) {
    // Never manufacture backfilled buckets after a long pause or outage.
    const uint32_t periods = uint32_t(now - bucketAt_) / kMotionBucketMs;
    if (periods == 1 && connected_ && bucketComplete_) {
      publishedMotion_ = bucketMotion_;
      motionReady_ = true;
    } else {
      motionReady_ = false;
    }
    bucketAt_ += periods * kMotionBucketMs;
    bucketMotion_ = 0;
    bucketComplete_ = connected_;
  }
  if (elapsed(now, healthAt_, kHealthMs)) {
    healthAt_ = now;
    healthReady_ = connected_;
  }
}
void Detector::sampleAlarm(uint16_t value, uint32_t now) {
  tick(now);
  if (value > kAlarmOn) {
    clearing_ = false;
    if (!alarm_) {
      alarm_ = true;
      if (!testSession_) { emit(EventKind::AlarmStart); alarmNotified_ = true; }
      change();
    }
    if (testSession_ && testState_ == TestState::Waiting) {
      testState_ = TestState::Detected;
      ++testSuccessSequence_;
      change();
    }
  } else if (alarm_) {
    if (value < kAlarmOff) {
      if (!clearing_) { clearing_ = true; clearAt_ = now; }
      if (elapsed(now, clearAt_, kAlarmClearMs)) clearAlarm();
    } else {
      clearing_ = false; // Hysteresis band cannot count toward continuous clearance.
    }
  }
}
void Detector::clearAlarm() {
  alarm_ = clearing_ = false;
  if (alarmNotified_) emit(EventKind::AlarmClear);
  alarmNotified_ = false;
  if (testState_ == TestState::Detected) testSession_ = false;
  if (silenceWaiting_ || silenceState_ == SilenceState::Confirm ||
      silenceState_ == SilenceState::StillDetected) {
    silenceWaiting_ = false;
    silenceState_ = SilenceState::Cleared;
  }
  change();
}
void Detector::sampleMotion(bool high, uint32_t now, uint32_t epoch) {
  tick(now);
  if (!pirReady_) {
    previousPir_ = high;
    if (elapsed(now, bootAt_, kPirWarmupMs)) pirReady_ = true;
    return;
  }
  if (high && !previousPir_) {
    increment(bucketMotion_);
    if (!connected_) increment(offlineMotion_);
    lastMotionEpoch_ = epoch;
  }
  previousPir_ = high;
}
void Detector::setConnected(bool connected, uint32_t now) {
  tick(now);
  if (connected_ == connected) return;
  connected_ = connected;
  if (!connected) {
    bucketComplete_ = false;
    motionReady_ = false;
    healthReady_ = false;
  }
  // Both edges cancel a pending confirmation. Never persist remote command state.
  if (silenceState_ == SilenceState::Confirm) silenceState_ = SilenceState::Idle;
  change();
}
bool Detector::requestTest(uint32_t now) {
  tick(now);
  if (!connected_ || alarm_ || testSession_ || buttonPressed_ || updating_) return false;
  testAt_ = pressAt_ = now;
  pressDuration_ = kTestPressMs;
  testSession_ = buttonPressed_ = true;
  testState_ = TestState::Waiting;
  silenceState_ = SilenceState::Idle;
  change();
  return true;
}
bool Detector::prepareSilence(uint32_t now) {
  tick(now);
  if (!connected_ || !alarm_ || testSession_ || buttonPressed_ || updating_ ||
      (everSilenced_ && !elapsed(now, silenceAt_, kSilenceCooldownMs))) return false;
  silenceState_ = SilenceState::Confirm;
  confirmAt_ = now;
  change();
  return true;
}
bool Detector::confirmSilence(uint32_t now) {
  tick(now);
  if (silenceState_ != SilenceState::Confirm || !connected_ || !alarm_ ||
      testSession_ || buttonPressed_ || updating_ ||
      (everSilenced_ && !elapsed(now, silenceAt_, kSilenceCooldownMs))) return false;
  silenceAt_ = pressAt_ = now;
  pressDuration_ = kSilencePressMs;
  everSilenced_ = silenceWaiting_ = buttonPressed_ = true;
  silenceState_ = SilenceState::Requested;
  change();
  return true;
}
bool Detector::beginOta(uint32_t now) {
  tick(now);
  if (!canOta()) return false;
  updating_ = true;
  buttonPressed_ = false;
  silenceState_ = SilenceState::Idle;
  change();
  return true;
}
const char* Detector::testText() const {
  switch (testState_) {
    case TestState::Waiting: return "Waiting for buzzer response";
    case TestState::Detected: return "Buzzer response detected";
    case TestState::Failed: return "No buzzer response detected";
    default: return "Not tested since restart";
  }
}
const char* Detector::silenceText() const {
  switch (silenceState_) {
    case SilenceState::Confirm: return "Confirm silence within 10 seconds";
    case SilenceState::Requested: return "Silence requested";
    case SilenceState::Cleared: return "Alarm cleared";
    case SilenceState::StillDetected: return "Alarm still detected";
    case SilenceState::Expired: return "Silence confirmation expired";
    default: return "No silence request pending";
  }
}
}  // namespace smoke
