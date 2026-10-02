# Implementation validation

Verified locally with Arduino CLI 1.3.1, ESP8266 core 3.1.2, Blynk 1.3.2 and
the NodeMCU `eesz=4M1M` build target. The compile used example credentials in a
separate staging directory. **No firmware has been uploaded to hardware.**

## Completed

- Host C++ tests: **9 scenario groups, 274 assertions passed**, compiled with
  warnings treated as errors. Includes a simulated 31-day routine-message budget.
- Full ESP8266 compile and link: **passed**, with `--warnings all`.
- Build memory report: static RAM **33,252 / 80,192 bytes (41%)**; instruction RAM
  **62,139 / 65,536 bytes (94%)**, including 32,768 bytes reserved for instruction
  cache; flash code **407,740 / 1,048,576 bytes (38%)**.
- No IFTTT/ThingSpeak clients, legacy notification API, blocking application
  delays, command synchronization, or insecure TLS mode remain in the firmware.

The profile-based first build could not download the ESP8266 archive in the
sandbox; the outside-sandbox download retry was declined. Target validation
instead used the already-installed **same core version** under Arduino15 and the
cached **same Blynk version**, with a staged sketch lacking the profile file and
an explicit FQBN/library path. This did not require further downloads or modify
the installed core. The normal build script still uses the pinned profile.

## Outstanding installation checks

The Blynk template/device/datastreams/dashboards and event notification recipients
must be configured in the user's account following README.md. Cloud delivery,
history widgets and monthly quota usage have not been verified against an account.

Actual board flash capacity, A0 voltage and thresholds, PNP button behavior, LED
boot behavior, PIR behavior, offline sensing, phone notifications, real OTA
authentication/interruption/repeated uploads and the 24-hour soak require the
physical-access session. Complete the README installation checklist before
closing the device. The host tests and target build do not replace these checks.
