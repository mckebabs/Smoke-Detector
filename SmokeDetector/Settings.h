#pragma once
#include <stdint.h>

namespace smoke {
constexpr uint8_t kButtonPin = 5;
constexpr uint8_t kPirPin = 12;
constexpr uint8_t kBlueLedPin = 2;
constexpr uint8_t kMotionLedPin = 15;
constexpr uint8_t kActivityLedPin = 13;
constexpr const char* kFirmwareVersion = "2.0.1";
constexpr const char* kHostname = "smoke-detector";
constexpr uint16_t kAlarmOn = 500;       // Active when ADC > this value.
constexpr uint16_t kAlarmOff = 450;      // Clear only while ADC < this value.
constexpr uint32_t kAlarmSampleMs = 100;
constexpr uint32_t kAlarmClearMs = 10000;
constexpr uint32_t kPirWarmupMs = 60000;
constexpr uint32_t kMotionBucketMs = 15UL * 60 * 1000;
constexpr uint32_t kHealthMs = 60UL * 60 * 1000;
constexpr uint32_t kTestPressMs = 2000;
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
static_assert(kAlarmOff < kAlarmOn, "ADC hysteresis thresholds must be ordered");
}  // namespace smoke
