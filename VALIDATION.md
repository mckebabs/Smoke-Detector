# Implementation validation

Verified locally with Arduino CLI 1.3.1, ESP8266 core 3.1.2, Blynk 1.3.5 and
the NodeMCU `eesz=4M1M` build target. The compile used example credentials in a
separate staging directory. **No firmware has been uploaded to hardware.**

## Completed

- Host C++ tests: **9 scenario groups, 274 assertions passed**, compiled with
  warnings treated as errors. Includes a simulated 31-day routine-message budget.
- Full ESP8266 compile and link: **passed**, with `--warnings all`.
- Build memory report: static RAM **32,736 / 80,192 bytes (40%)**; instruction RAM
  **62,139 / 65,536 bytes (94%)**, including 32,768 bytes reserved for instruction
  cache; flash code **407,116 / 1,048,576 bytes (38%)**.
- Firmware 2.0.1 binary: **445,488 bytes**, built with example credentials only;
  do not upload this validation binary.
- Blynk 1.3.5 integration: protocol timeout updated to the supported 1,000 ms
  minimum; separate DNS/TCP/socket timeout remains 300 ms. Current Blynk CA
  bundle is loaded with hostname and validity verification retained.
- No IFTTT/ThingSpeak clients, legacy notification API, blocking application
  delays, command synchronization, or insecure TLS mode remain in the firmware.

Target validation used the installed ESP8266 core 3.1.2 under Arduino15 and the
user's installed Blynk 1.3.5 library, with a staged sketch lacking the profile
file and an explicit FQBN/library path. The normal build script uses the updated
pinned profile. Host tests were rerun after the compatibility changes.

## Outstanding installation checks

The user reports that the web/iOS dashboards have been configured. Cloud delivery,
history widgets and monthly quota usage have not been verified by these code checks.

Actual board flash capacity, A0 voltage and thresholds, PNP button behavior, LED
boot behavior, PIR behavior, offline sensing, phone notifications, real OTA
authentication/interruption/repeated uploads and the 24-hour soak require the
physical-access session. Complete the README installation checklist before
closing the device. The host tests and target build do not replace these checks.
