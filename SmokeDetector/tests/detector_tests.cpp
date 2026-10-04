#include "Detector.h"
#include "UpdateMode.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

using namespace smoke;
static int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static Detector online(uint32_t now = 0) {
  Detector d; d.begin(now); d.setConnected(true, now); return d;
}

static void alarm_hysteresis_and_beeps() {
  Detector d; d.begin(0);
  d.sampleAlarm(500, 0); CHECK(!d.alarmActive());
  d.sampleAlarm(501, 100); CHECK(d.alarmActive());
  CHECK(d.events.size() == 1); CHECK(d.events.front()->occurredOffline);
  d.sampleAlarm(900, 200); CHECK(d.events.size() == 1);
  d.sampleAlarm(449, 300);
  d.sampleAlarm(449, 10299); CHECK(d.alarmActive());
  d.sampleAlarm(450, 10300); // equality/band interrupts continuous clearance
  d.sampleAlarm(449, 10400);
  d.sampleAlarm(600, 15000); // another beep interrupts clearance
  d.sampleAlarm(449, 16000);
  d.sampleAlarm(449, 26000); CHECK(!d.alarmActive());
  CHECK(d.events.size() == 2);
  d.events.pop(); CHECK(d.events.front()->kind == EventKind::AlarmClear);
  d.sampleAlarm(501, 26100); CHECK(d.alarmActive()); CHECK(d.events.size() == 2);
}

static void tests_and_escalation() {
  Detector d = online();
  CHECK(d.requestTest(0)); CHECK(d.buttonPressed()); CHECK(!d.canOta());
  CHECK(!d.requestTest(1)); CHECK(!d.prepareSilence(1));
  d.tick(1999); CHECK(d.buttonPressed());
  d.tick(2000); CHECK(!d.buttonPressed());
  d.sampleAlarm(600, 2500); CHECK(d.alarmActive());
  CHECK(d.testState() == TestState::Detected); CHECK(d.events.size() == 0);
  d.recordTestEpoch(1800000000); CHECK(d.lastTestEpoch() == 1800000000);
  d.sampleAlarm(0, 3000); d.sampleAlarm(0, 13000);
  CHECK(!d.testRunning()); CHECK(!d.alarmActive()); CHECK(d.events.size() == 0);
  CHECK(d.requestTest(14000)); d.sampleAlarm(800, 15000);
  d.tick(28999); CHECK(d.events.size() == 0);
  d.tick(29000); CHECK(!d.testRunning()); CHECK(d.alarmActive());
  CHECK(d.events.size() == 1); CHECK(d.events.front()->kind == EventKind::AlarmStart);
  CHECK(!d.requestTest(29001));
  d.sampleAlarm(0, 30000); d.sampleAlarm(0, 40000);
  CHECK(d.events.size() == 2);

  Detector f = online(); CHECK(f.requestTest(0));
  f.tick(9999); CHECK(f.testState() == TestState::Waiting);
  f.tick(10000); CHECK(f.testState() == TestState::Failed); CHECK(!f.testRunning());
  CHECK(!f.buttonPressed()); CHECK(f.events.front()->kind == EventKind::TestFailed);
  f.sampleAlarm(600, 10100); CHECK(f.events.size() == 2); // late response is a real alarm

  Detector offline; offline.begin(0); CHECK(!offline.requestTest(0));
}

static void silence_confirmation_and_recurrence() {
  Detector d = online(); CHECK(!d.prepareSilence(0));
  d.sampleAlarm(700, 100);
  CHECK(!d.confirmSilence(101)); CHECK(d.prepareSilence(200));
  d.tick(10200); CHECK(d.silenceState() == SilenceState::Expired);
  CHECK(!d.confirmSilence(10200)); CHECK(d.prepareSilence(10300));
  CHECK(d.confirmSilence(10400)); CHECK(d.alarmActive()); CHECK(d.buttonPressed());
  CHECK(!d.confirmSilence(10401)); CHECK(!d.requestTest(10401)); CHECK(!d.canOta());
  d.tick(11400); CHECK(!d.buttonPressed()); CHECK(d.alarmActive());
  d.tick(25400); CHECK(d.silenceState() == SilenceState::StillDetected);
  CHECK(!d.prepareSilence(40399)); CHECK(d.prepareSilence(40400));
  CHECK(d.confirmSilence(40500));
  d.sampleAlarm(0, 41000); d.sampleAlarm(0, 51000);
  CHECK(!d.alarmActive()); CHECK(d.silenceState() == SilenceState::Cleared);
  CHECK(d.events.size() == 2); // silence never manufactures clearance
  d.sampleAlarm(700, 51100); CHECK(d.events.size() == 3);

  Detector c = online(); c.sampleAlarm(700, 0); CHECK(c.prepareSilence(1));
  c.setConnected(false, 2); CHECK(!c.confirmSilence(3));
  c.setConnected(true, 4); CHECK(!c.confirmSilence(5));
  CHECK(!c.buttonPressed()); CHECK(c.prepareSilence(6));
  c.begin(7); c.setConnected(true, 7); CHECK(!c.confirmSilence(8));
  CHECK(!c.buttonPressed()); CHECK(!c.testRunning());

  Detector a = online(); a.sampleAlarm(700, 0); CHECK(a.prepareSilence(1));
  a.sampleAlarm(0, 2); a.sampleAlarm(0, 10002);
  CHECK(!a.confirmSilence(10003));
}

static void offline_local_monitoring_and_queue() {
  Detector d; d.begin(0); d.sampleAlarm(700, 100);
  CHECK(d.alarmActive()); CHECK(!d.prepareSilence(101));
  d.sampleAlarm(0, 200); d.sampleAlarm(0, 10200);
  CHECK(!d.alarmActive()); CHECK(d.events.size() == 2);
  d.setConnected(true, 10300); CHECK(d.events.size() == 2);
  CHECK(d.events.front()->atMs == 100);

  EventQueue q;
  for (uint8_t i = 0; i < kEventCapacity; ++i) q.push({EventKind::AlarmClear, i, true});
  q.push({EventKind::TestFailed, 100, true}); CHECK(q.dropped() == 1);
  q.push({EventKind::AlarmStart, 101, true}); CHECK(q.dropped() == 2);
  CHECK(q.size() == kEventCapacity);
  for (uint8_t i = 1; i < kEventCapacity; ++i) {
    CHECK(q.front()->kind == EventKind::AlarmClear); CHECK(q.front()->atMs == i); q.pop();
  }
  CHECK(q.front()->kind == EventKind::AlarmStart); q.pop(); CHECK(!q.front());
  q.acknowledgeDropped(2); CHECK(q.dropped() == 0);
  for (uint8_t i = 0; i < kEventCapacity; ++i) q.push({EventKind::AlarmStart, i, true});
  q.push({EventKind::AlarmStart, 99, true}); CHECK(q.front()->atMs == 1);
  CHECK(q.size() == kEventCapacity); CHECK(q.dropped() == 1);
}

static void motion_buckets_and_outages() {
  Detector d = online();
  d.sampleMotion(true, 1000, 1800000000); // ignored during warm-up
  d.sampleMotion(true, 60000, 1800000001); // already high at warm-up: do not invent an edge
  d.sampleMotion(true, 60100, 1800000002); CHECK(d.lastMotionEpoch() == 0);
  d.sampleMotion(false, 60200, 0); d.sampleMotion(true, 60300, 1800000003);
  CHECK(d.lastMotionEpoch() == 1800000003);
  d.tick(kMotionBucketMs); CHECK(!d.motionReady()); // first bucket contains startup offline gap
  d.sampleMotion(false, kMotionBucketMs + 20, 0);
  d.sampleMotion(true, kMotionBucketMs + 40, 1800000004);
  d.sampleMotion(true, kMotionBucketMs + 60, 1800000005);
  d.tick(2 * kMotionBucketMs); CHECK(d.motionReady()); CHECK(d.motionTotal() == 1);
  d.acknowledgeMotion();
  d.tick(3 * kMotionBucketMs); CHECK(d.motionReady()); CHECK(d.motionTotal() == 0);
  d.acknowledgeMotion();
  d.setConnected(false, 3 * kMotionBucketMs + 100);
  d.sampleMotion(false, 3 * kMotionBucketMs + 120, 0);
  d.sampleMotion(true, 3 * kMotionBucketMs + 140, 1800000006);
  CHECK(d.offlineMotion() == 1);
  d.tick(4 * kMotionBucketMs); CHECK(!d.motionReady());
  d.setConnected(true, 4 * kMotionBucketMs + 100);
  d.tick(5 * kMotionBucketMs); CHECK(!d.motionReady());
  d.tick(6 * kMotionBucketMs); CHECK(d.motionReady()); CHECK(d.motionTotal() == 0);
  d.acknowledgeOfflineMotion(1); CHECK(d.offlineMotion() == 0);
  d.tick(2 * kHealthMs); CHECK(d.healthReady());
  d.acknowledgeHealth(); CHECK(!d.healthReady());
  d.setConnected(false, 2 * kHealthMs + 1);
  d.tick(3 * kHealthMs); CHECK(!d.healthReady());
}

static void rollover_and_backoff() {
  const uint32_t start = UINT32_MAX - 500;
  Detector d = online(start); CHECK(d.requestTest(start));
  d.tick(start + 1999U); CHECK(d.buttonPressed());
  d.tick(start + 2000U); CHECK(!d.buttonPressed()); CHECK(d.uptimeMs() == 2000);
  d.tick(start + 10000U); CHECK(d.testState() == TestState::Failed);
  d.sampleAlarm(700, start + 10100U); d.sampleAlarm(0, start + 10200U);
  d.sampleAlarm(0, start + 20200U); CHECK(!d.alarmActive()); CHECK(d.uptimeMs() == 20200);

  Backoff retry; uint32_t now = start; CHECK(retry.ready(now));
  uint32_t expected = kReconnectMinMs;
  for (int i = 0; i < 10; ++i) {
    retry.attempted(now); CHECK(!retry.ready(now + expected - 1));
    now += expected; CHECK(retry.ready(now));
    expected = expected < kReconnectMaxMs / 2 ? expected * 2 : kReconnectMaxMs;
  }
  retry.reset(); CHECK(retry.ready(now));
}

static void ota_admission() {
  Detector d = online(); CHECK(d.canOta()); CHECK(d.beginOta(0));
  CHECK(!d.requestTest(1)); CHECK(!d.beginOta(1));
  d.endOta(); CHECK(d.requestTest(2)); CHECK(!d.beginOta(3));
  d.tick(10002); CHECK(d.beginOta(10003)); d.endOta();
  d.sampleAlarm(800, 10004); CHECK(!d.beginOta(10005));
  CHECK(d.prepareSilence(10006)); CHECK(d.confirmSilence(10007));
  CHECK(!d.beginOta(10008));
}

static void routine_budget() {
  Detector d = online();
  uint32_t motionWrites = 0, healthWrites = 0;
  // Fast simulated month: advance by minute, draining readiness each step.
  for (uint32_t now = 60000; now <= 31UL * 86400000; now += 60000) {
    d.tick(now);
    if (d.motionReady()) { ++motionWrites; d.acknowledgeMotion(); }
    if (d.healthReady()) { healthWrites += 4; d.acknowledgeHealth(); }
  }
  CHECK(motionWrites == 2975); CHECK(healthWrites == 2976);
  CHECK(motionWrites + healthWrites <= 5952);
}

static void event_limits_and_retry_order() {
  EventLimiter limiter;
  CHECK(limiter.ready(0)); limiter.record(0);
  CHECK(!limiter.ready(kEventSpacingMs - 1));
  for (uint16_t i = 1; i < kEventDailyLimit; ++i) {
    const uint64_t at = uint64_t(i) * kEventSpacingMs;
    CHECK(limiter.ready(at)); limiter.record(at);
  }
  CHECK(!limiter.ready(86400000ULL - 1)); CHECK(limiter.ready(86400000ULL));
  limiter.record(86400000ULL); CHECK(!limiter.ready(86400001ULL));
  CHECK(limiter.ready(86400000ULL + kEventSpacingMs));
  EventQueue q;
  q.push({EventKind::AlarmClear, 20, false});
  q.push({EventKind::AlarmStart, 10, true}, true);
  CHECK(q.front()->atMs == 10); q.pop(); CHECK(q.front()->atMs == 20);
}

static void update_window_and_physical_activation() {
  UpdateMode mode;
  CHECK(!mode.request(0, false)); CHECK(!mode.busy());
  CHECK(mode.request(0, true)); CHECK(!mode.request(1, true));
  CHECK(!mode.ready(999)); CHECK(mode.ready(1000));
  mode.started(1000); CHECK(mode.active()); CHECK(!mode.pending());
  CHECK(!mode.request(2000, true));
  CHECK(!mode.expired(300999)); CHECK(mode.expired(301000));
  mode.stop(); CHECK(!mode.busy()); CHECK(!mode.expired(400000));
  CHECK(mode.request(UINT32_MAX - 500, true)); CHECK(mode.ready(499));
  mode.started(UINT32_MAX - 1000);
  CHECK(!mode.expired(298998)); CHECK(mode.expired(298999));
  mode.stop();
  CHECK(!mode.sampleButton(true, 0)); CHECK(!mode.sampleButton(true, 10000));
  CHECK(!mode.sampleButton(false, 10001));
  CHECK(!mode.sampleButton(true, 10002)); CHECK(!mode.sampleButton(true, 13001));
  CHECK(mode.sampleButton(true, 13002)); CHECK(!mode.sampleButton(true, 50000));
  CHECK(!mode.sampleButton(false, 50001));
  CHECK(!mode.sampleButton(true, UINT32_MAX - 1000));
  CHECK(mode.sampleButton(true, 1999));
  Detector d = online(); d.sampleAlarm(600, 0);
  CHECK(!mode.request(0, d.canOta()));
  d.sampleAlarm(0, 1); d.sampleAlarm(0, 10001);
  CHECK(d.requestTest(10002)); CHECK(!mode.request(10003, d.canOta()));
  d.tick(20002); CHECK(mode.request(20003, d.canOta()));
  d.sampleAlarm(600, 20004); CHECK(!d.canOta());
  mode.stop(); CHECK(!mode.ready(21003));
}

int main() {
  alarm_hysteresis_and_beeps(); tests_and_escalation();
  silence_confirmation_and_recurrence(); offline_local_monitoring_and_queue();
  motion_buckets_and_outages(); rollover_and_backoff(); ota_admission(); routine_budget();
  event_limits_and_retry_order();
  update_window_and_physical_activation();
  printf("PASS: 10 scenario groups, %d assertions.\n", assertions);
}
