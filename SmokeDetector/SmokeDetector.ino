// Modern Blynk + local OTA. See README.md before installing on the detector.
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#error "Copy Secrets.example.h to Secrets.h and configure Wi-Fi, Blynk and OTA."
#endif

// Blynk 1.3.5 requires a protocol timeout of at least 1000 ms.
// DNS/TCP/socket operations retain their separate 300 ms transport timeout.
#define BLYNK_TIMEOUT_MS 1000
#define BLYNK_HEARTBEAT 60
#define BLYNK_NO_DEFAULT_BANNER
#define BLYNK_PRINT Serial
#include "CloudTransport.h"
#include <ArduinoOTA.h>
#include <Schedule.h>
#include <time.h>
#include "Detector.h"

static_assert(sizeof(SMOKE_OTA_PASSWORD) >= 13, "Use an OTA password of at least 12 characters");
static_assert(sizeof(BLYNK_AUTH_TOKEN) > 1, "Blynk token must not be empty");

smoke::Detector detector;
BoundedTlsClient cloudSocket;
CloudTransport cloudTransport(cloudSocket);
CloudClient Blynk(cloudTransport);
#include <BlynkWidgets.h>
BearSSL::X509List cloudTrust(kCloudRootCa);
smoke::Backoff wifiRetry, cloudRetry;

bool otaListening = false, otaUpdating = false, otaPermitted = false;
bool recurrentSensors = false, inputsBusy = false, rawPir = false, otaHandling = false;
uint16_t lastAdc = 0, adcMin = 1023, adcMax = 0;
uint32_t alarmSampleAt = 0, motionSampleAt = 0, activityAt = 0;
bool activityPulse = false;
uint32_t diagnosticAt = 0, writesSent = 0, eventsSent = 0;
uint32_t maximumSampleGap = 0, testSequenceSeen = 0;
bool cloudWasConnected = false;
uint64_t outageAt = 0, lastOutageSeconds = 0;
uint32_t pending = 0, publishAt = 0;
int lastAlarm = -1, lastTest = -1, lastSilence = -1;
uint32_t lastTestEpoch = UINT32_MAX;
smoke::EventLimiter eventLimiter;
char restartReason[128]{};

enum PublishBit : uint8_t {
  AlarmBit, TestBit, TestEpochBit, SilenceBit, RssiBit, UptimeBit, HeapBit,
  MotionTimeBit, VersionBit, ResetBit, MotionBucketBit, SummaryBit
};
constexpr uint32_t kHealthMask = (1UL << RssiBit) | (1UL << UptimeBit) |
    (1UL << HeapBit) | (1UL << MotionTimeBit);
constexpr uint32_t kSnapshotMask = (1UL << (ResetBit + 1)) - 1;

uint32_t utcNow() {
  const time_t value = time(nullptr);
  return value >= 1704067200 ? static_cast<uint32_t>(value) : 0;
}

void updateOutputs(uint32_t now) {
  digitalWrite(smoke::kButtonPin, detector.buttonPressed() ? LOW : HIGH);
  digitalWrite(smoke::kBlueLedPin, detector.alarmActive() ? LOW : HIGH);
  digitalWrite(smoke::kMotionLedPin, (rawPir || detector.alarmActive()) ? HIGH : LOW);
  const bool pulse = activityPulse && !smoke::elapsed(now, activityAt, 50);
  const bool offlineBlink = !detector.connected() && now % 2000 < 50;
  digitalWrite(smoke::kActivityLedPin, (detector.alarmActive() || pulse || offlineBlink) ? HIGH : LOW);
}

// Runs in CONT context, including during network yields. No networking or logs.
void serviceInputs() {
  if (inputsBusy || otaUpdating || otaHandling) return;
  inputsBusy = true;
  const uint32_t now = millis();
  detector.tick(now);
  if (smoke::elapsed(now, alarmSampleAt, smoke::kAlarmSampleMs)) {
    const uint32_t gap = uint32_t(now - alarmSampleAt);
    if (gap > maximumSampleGap) maximumSampleGap = gap;
    alarmSampleAt = now;
    lastAdc = analogRead(A0);
    if (lastAdc < adcMin) adcMin = lastAdc;
    if (lastAdc > adcMax) adcMax = lastAdc;
    detector.sampleAlarm(lastAdc, now);
  }
  if (smoke::elapsed(now, motionSampleAt, 20)) {
    motionSampleAt = now;
    rawPir = digitalRead(smoke::kPirPin) == HIGH;
    detector.sampleMotion(rawPir, now, utcNow());
  }
  if (detector.testSuccessSequence() != testSequenceSeen) {
    testSequenceSeen = detector.testSuccessSequence();
    detector.recordTestEpoch(utcNow());
  }
  updateOutputs(now);
  inputsBusy = false;
}

void observeConnection(bool connected) {
  if (connected == cloudWasConnected) return;
  detector.setConnected(connected, millis());
  cloudWasConnected = connected;
  if (connected) {
    lastOutageSeconds = (detector.uptimeMs() - outageAt) / 1000;
    pending = kSnapshotMask;
    // Retain the last successful timestamp in Blynk across firmware restarts.
    if (!detector.lastTestEpoch()) pending &= ~(1UL << TestEpochBit);
    if (lastOutageSeconds || detector.offlineMotion() || detector.events.dropped()) pending |= 1UL << SummaryBit;
    cloudRetry.reset();
    Serial.println(F("Blynk connected; publishing state, never syncing commands."));
  } else {
    outageAt = detector.uptimeMs();
    pending = 0;
    Blynk.disconnect();
    Serial.println(F("Blynk disconnected; sensors and local OTA remain independent."));
  }
}

BLYNK_CONNECTED() { observeConnection(true); }
BLYNK_DISCONNECTED() { observeConnection(false); }

BLYNK_WRITE(V0) {
  serviceInputs();
  if (param.asInt() == 1) {
    const bool accepted = detector.requestTest(millis());
    Serial.println(accepted ? F("Test accepted.") : F("Test rejected: busy or alarming."));
    updateOutputs(millis());
  }
}
BLYNK_WRITE(V11) {
  serviceInputs();
  if (param.asInt() == 1) {
    const bool accepted = detector.prepareSilence(millis());
    Serial.println(accepted ? F("Silence confirmation opened.") : F("Silence preparation rejected."));
  }
}
BLYNK_WRITE(V12) {
  serviceInputs();
  if (param.asInt() == 1) {
    const bool accepted = detector.confirmSilence(millis());
    Serial.println(accepted ? F("Silence requested.") : F("Silence confirmation rejected."));
    updateOutputs(millis());
  }
}

void configureOta() {
  ArduinoOTA.setHostname(smoke::kHostname);
  ArduinoOTA.setPassword(SMOKE_OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    otaUpdating = detector.beginOta(millis());
    digitalWrite(smoke::kButtonPin, HIGH);
    Serial.println(F("OTA started; button released, ESP monitoring paused."));
  });
  ArduinoOTA.onEnd([]() { Serial.println(F("OTA complete; rebooting.")); });
  ArduinoOTA.onError([](ota_error_t error) {
    otaUpdating = false;
    detector.endOta();
    digitalWrite(smoke::kButtonPin, HIGH);
    Serial.printf("OTA failed (code %u); monitoring resumed.\n", unsigned(error));
  });
  const uint32_t actual = ESP.getFlashChipRealSize(), configured = ESP.getFlashChipSize();
  otaPermitted = actual == configured && ESP.getFreeSketchSpace() >= ESP.getSketchSize();
  Serial.printf("Flash actual/configured: %lu/%lu; sketch/free OTA: %lu/%lu; OTA %s\n",
      static_cast<unsigned long>(actual), static_cast<unsigned long>(configured),
      static_cast<unsigned long>(ESP.getSketchSize()), static_cast<unsigned long>(ESP.getFreeSketchSpace()),
      otaPermitted ? "eligible" : "DISABLED");
}

void serviceOta() {
  // Remove the listener during alarms/tests, discarding pending uploads too.
  // onStart alone is too late: ArduinoOTA 3.1.2 calls it after Update.begin().
  const bool allowed = otaPermitted && WiFi.status() == WL_CONNECTED && detector.canOta();
  if (!allowed && otaListening) {
    ArduinoOTA.end(); otaListening = false;
  } else if (allowed && !otaListening) {
    ArduinoOTA.begin(); otaListening = true;
  }
  if (allowed && otaListening) {
    // Freeze admission conditions across handle(): even its setup/MDNS yields
    // must not change the model between eligibility and the start callback.
    otaHandling = true;
    ArduinoOTA.handle();
    otaHandling = false;
  }
}

void serviceNetwork() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (cloudWasConnected) observeConnection(false);
    if (wifiRetry.ready(now)) {
      wifiRetry.attempted(now);
      WiFi.begin(SMOKE_WIFI_SSID, SMOKE_WIFI_PASSWORD);
    }
    return;
  }
  wifiRetry.reset();
  if (Blynk.connected()) {
    Blynk.run();
    if (!Blynk.connected()) observeConnection(false);
  } else {
    if (cloudWasConnected) observeConnection(false);
    // TLS needs a clock. SNTP runs in the background without a wait loop.
    if (utcNow() && recurrentSensors && !detector.buttonPressed() &&
        !detector.testRunning() && cloudRetry.ready(now)) {
      const uint32_t started = millis();
      const bool connected = Blynk.connect(smoke::kCloudConnectTimeoutMs);
      if (connected) observeConnection(true);
      else {
        const int tlsError = cloudSocket.getLastSSLError();
        const bool invalidToken = Blynk.isTokenInvalid();
        Blynk.disconnect();
        cloudRetry.attempted(millis());
        Serial.printf("Blynk attempt failed after %lu ms; TLS error %d; invalid token %u. Retrying with backoff.\n",
            static_cast<unsigned long>(uint32_t(millis() - started)),
            tlsError, unsigned(invalidToken));
      }
    }
  }
}

void markPublications() {
  if (!Blynk.connected()) return;
  if (lastAlarm != int(detector.alarmActive())) pending |= 1UL << AlarmBit;
  if (lastTest != int(detector.testState())) pending |= 1UL << TestBit;
  if (detector.lastTestEpoch() && lastTestEpoch != detector.lastTestEpoch()) pending |= 1UL << TestEpochBit;
  if (lastSilence != int(detector.silenceState())) pending |= 1UL << SilenceBit;
  if (detector.healthReady()) { pending |= kHealthMask; detector.acknowledgeHealth(); }
  if (detector.motionReady()) pending |= 1UL << MotionBucketBit;
  else pending &= ~(1UL << MotionBucketBit);
  if (detector.events.dropped() || detector.offlineMotion()) pending |= 1UL << SummaryBit;
}

void pulseActivity() { activityPulse = true; activityAt = millis(); }

void publishOne(uint8_t bit) {
  // Capture state before virtualWrite: network yields may change the model.
  switch (bit) {
    case AlarmBit: {
      const int value = detector.alarmActive();
      Blynk.virtualWrite(V1, value); lastAlarm = value; break;
    }
    case TestBit: {
      const auto state = detector.testState();
      Blynk.virtualWrite(V7, detector.testText()); lastTest = int(state); break;
    }
    case TestEpochBit: {
      const uint32_t value = detector.lastTestEpoch();
      Blynk.virtualWrite(V8, value); lastTestEpoch = value; break;
    }
    case SilenceBit: {
      const auto state = detector.silenceState();
      Blynk.virtualWrite(V13, detector.silenceText()); lastSilence = int(state); break;
    }
    case RssiBit: Blynk.virtualWrite(V3, WiFi.RSSI()); break;
    case UptimeBit: {
      char value[24];
      snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(detector.uptimeMs() / 1000));
      Blynk.virtualWrite(V4, value); break;
    }
    case HeapBit: Blynk.virtualWrite(V5, ESP.getFreeHeap()); break;
    case MotionTimeBit: Blynk.virtualWrite(V6, detector.lastMotionEpoch()); break;
    case VersionBit: Blynk.virtualWrite(V9, smoke::kFirmwareVersion); break;
    case ResetBit: Blynk.virtualWrite(V10, restartReason); break;
    case MotionBucketBit: {
      const uint32_t value = detector.motionTotal();
      Blynk.virtualWrite(V2, value);
      if (Blynk.connected()) detector.acknowledgeMotion();
      break;
    }
    case SummaryBit: {
      const uint32_t motion = detector.offlineMotion(), dropped = detector.events.dropped();
      char text[192];
      snprintf(text, sizeof(text), "Last outage: %llu s; offline motion: %lu; dropped event records: %lu. RAM since restart.",
          static_cast<unsigned long long>(lastOutageSeconds),
          static_cast<unsigned long>(motion), static_cast<unsigned long>(dropped));
      Blynk.virtualWrite(V14, text);
      if (Blynk.connected()) {
        detector.acknowledgeOfflineMotion(motion);
        detector.events.acknowledgeDropped(dropped);
      }
      break;
    }
  }
  ++writesSent;
  pulseActivity();
}

bool eventAllowed() {
  return eventLimiter.ready(detector.uptimeMs());
}

void publishEvent() {
  const smoke::Event* next = detector.events.front();
  if (!next || !eventAllowed()) return;
  const smoke::Event event = *next;
  const char* code = event.kind == smoke::EventKind::AlarmStart ? "smoke_alarm" :
      event.kind == smoke::EventKind::AlarmClear ? "smoke_clear" : "test_failed";
  const char* label = event.kind == smoke::EventKind::AlarmStart ? "Buzzer alarm detected" :
      event.kind == smoke::EventKind::AlarmClear ? "Buzzer alarm cleared" : "No test buzzer response";
  char description[192];
  snprintf(description, sizeof(description), "%s; occurred %llu seconds ago%s; alarm now %s.",
      label, static_cast<unsigned long long>((detector.uptimeMs() - event.atMs) / 1000),
      event.occurredOffline ? " while offline" : "", detector.alarmActive() ? "ACTIVE" : "clear");
  // Pop before network yields so queue eviction cannot remove a different event.
  // logEvent() has no application-level acknowledgement; delivery is best effort.
  detector.events.pop();
  Blynk.logEvent(code, description);
  eventLimiter.record(detector.uptimeMs());
  if (!Blynk.connected() || !cloudSocket.connected()) {
    detector.events.push(event, true);
    observeConnection(false);
  }
  ++eventsSent;
  pulseActivity();
}

void serviceTelemetry() {
  markPublications();
  if (!Blynk.connected() || !smoke::elapsed(millis(), publishAt, 250)) return;
  publishAt = millis();
  if (pending & (1UL << AlarmBit)) {
    pending &= ~(1UL << AlarmBit); publishOne(AlarmBit);
  } else if (detector.events.front() && eventAllowed()) {
    publishEvent();
  } else {
    for (uint8_t bit = 1; bit <= SummaryBit; ++bit) {
      if (pending & (1UL << bit)) {
        pending &= ~(1UL << bit); publishOne(bit); break;
      }
    }
  }
}

void setup() {
  digitalWrite(smoke::kButtonPin, HIGH);
  pinMode(smoke::kButtonPin, OUTPUT);
  pinMode(smoke::kPirPin, INPUT);
  pinMode(smoke::kBlueLedPin, OUTPUT);
  pinMode(smoke::kMotionLedPin, OUTPUT);
  pinMode(smoke::kActivityLedPin, OUTPUT);
  Serial.begin(115200);
  const uint32_t now = millis();
  detector.begin(now);
  alarmSampleAt = now - smoke::kAlarmSampleMs;
  motionSampleAt = now - 20;
  serviceInputs();
  recurrentSensors = schedule_recurrent_function_us([]() { serviceInputs(); return true; }, 20000);
  if (!recurrentSensors) Serial.println(F("Sensor scheduling failed; cloud connections disabled."));
  snprintf(restartReason, sizeof(restartReason), "%s", ESP.getResetReason().c_str());
  configureOta();
  cloudSocket.setTrustAnchors(&cloudTrust);
  cloudSocket.setBufferSizes(16384, 512); // Do not require server MFLN support.
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.disconnect();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(smoke::kHostname);
  WiFi.setAutoReconnect(false);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.printf("Smoke detector %s ready. OTA hostname: %s.\n", smoke::kFirmwareVersion, smoke::kHostname);
}

void loop() {
  serviceInputs();
  serviceOta();
  serviceNetwork();
  serviceInputs();
  serviceOta();
  serviceTelemetry();
  if (smoke::elapsed(millis(), diagnosticAt, 60000)) {
    diagnosticAt = millis();
    Serial.printf("ADC last/min/max %u/%u/%u; max sample gap %lu ms; alarm %u; heap %lu; writes/events %lu/%lu; WiFi %u; clock %u; Blynk %u\n",
        unsigned(lastAdc), unsigned(adcMin), unsigned(adcMax),
        static_cast<unsigned long>(maximumSampleGap), unsigned(detector.alarmActive()),
        static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(writesSent),
        static_cast<unsigned long>(eventsSent), unsigned(WiFi.status()),
        unsigned(utcNow() != 0), unsigned(Blynk.connected()));
    adcMin = 1023; adcMax = 0; maximumSampleGap = 0;
  }
}
