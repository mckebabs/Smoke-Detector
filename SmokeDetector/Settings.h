#pragma once
#include <stdint.h>

namespace smoke {
constexpr uint8_t kButtonPin = 5;
constexpr uint8_t kPirPin = 12;
constexpr uint8_t kBlueLedPin = 2;
constexpr uint8_t kMotionLedPin = 15;
constexpr uint8_t kActivityLedPin = 13;
constexpr const char* kFirmwareVersion = "2.0.5";
constexpr const char* kHostname = "smoke-detector";
constexpr const char* kUpdateSsid = "smoke-detector-direct";
constexpr uint8_t kUpdateButtonPin = 0; // NodeMCU FLASH; press after startup.
constexpr uint32_t kUpdateHoldMs = 3000;
constexpr uint32_t kUpdateStartDelayMs = 1000;
constexpr uint32_t kUpdateWindowMs = 5UL * 60 * 1000;
constexpr uint16_t kAlarmOn = 500;       // Active when ADC > this value.
constexpr uint16_t kAlarmOff = 450;      // Clear only while ADC < this value.
constexpr uint32_t kAlarmSampleMs = 100;
constexpr uint32_t kAlarmClearMs = 10000;
constexpr uint32_t kPirWarmupMs = 60000;
constexpr uint32_t kMotionBucketMs = 15UL * 60 * 1000;
constexpr uint32_t kHealthMs = 60UL * 60 * 1000;
constexpr uint32_t kTestPressMs = 5000;
constexpr uint32_t kTestResponseMs = 10000;
constexpr uint32_t kTestEscalateMs = 15000;
constexpr uint32_t kSilencePressMs = 1000;
constexpr uint32_t kSilenceConfirmMs = 10000;
constexpr uint32_t kSilenceCooldownMs = 30000;
constexpr uint32_t kSilenceResultMs = 15000;
constexpr uint8_t kEventCapacity = 16;
constexpr uint32_t kEventSpacingMs = 6000;
constexpr uint16_t kEventDailyLimit = 90; // Leave room below Blynk's 100/day cap.
constexpr uint32_t kReconnectMinMs = 1000;
constexpr uint32_t kReconnectMaxMs = 60000;
constexpr uint32_t kNetworkTimeoutMs = 300;
// Blynk's connect deadline includes DNS, TCP, TLS and login processing.
// The core can spend up to 15 seconds in a TLS handshake.
constexpr uint32_t kCloudConnectTimeoutMs = 20000;
static_assert(kAlarmOff < kAlarmOn, "ADC hysteresis thresholds must be ordered");
static_assert(kTestPressMs < kTestResponseMs, "Release the test button before the response deadline");
}  // namespace smoke
