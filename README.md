# Smoke Detector 2.0

ESP8266/NodeMCU retrofit for an existing smoke detector: modern Blynk, PIR history,
remote test and confirmed silence, health telemetry, and local password-protected
ArduinoOTA. IFTTT, ThingSpeak, daily motion pushes, and away mode have been removed.

In a GitHub checkout, firmware, configuration examples, build scripts and tests
live in `SmokeDetector/`. Run the build/test commands below from that directory
(`cd SmokeDetector` from the repository root). This README and `VALIDATION.md`
are at the repository root.

The original hardware demonstration is available in the
[project video](https://www.youtube.com/watch?v=8DJ84n9dWVM); it shows the legacy
app and firmware, rather than this refactor's controls.

## Hardware and operation

| Connection | GPIO | Behavior |
| --- | --- | --- |
| Filtered detector buzzer | A0 | Read every 100 ms; alarm above 500 |
| Detector button via existing PNP circuit | 5 / D1 | HIGH released, LOW pressed |
| HC-S501 PIR | 12 / D6 | Rising-edge count after 60-second warm-up |
| Blue LED | 2 / D4 | Active LOW during alarm |
| Motion LED | 15 / D8 | Raw PIR activity or alarm |
| Activity LED | 13 / D7 | Transmission pulses; offline blink; steady during alarm |

The existing circuit and 5 V to 9 V booster are retained. Verify the actual A0
voltage limit of this NodeMCU and the filter output before installation; the
ESP8266 chip itself accepts a smaller range than some development-board A0 inputs.
GPIO15 is a boot strap: validate the existing LED wiring permits a normal boot.

Alarm clearance requires ten continuous seconds below 450. A reading in the
450–500 band interrupts clearance. This holds an episode across buzzer gaps.
Thresholds and all durations live in `Settings.h` and need a real buzzer check.
The firmware senses a buzzer signal, not smoke concentration.

Sensor and button processing also runs in a recurrent ESP8266 CONT-stack callback
during network yields. DNS and TCP use a 300 ms timeout; the core's TLS handshake
can take up to 15 seconds. Blynk's protocol timeout is separately set to 1 second,
the minimum supported by Blynk 1.3.5. Failed cloud attempts back off from 1 to 60 seconds
after completion. The overall connection deadline is 20 seconds so a TLS
handshake does not consume the entire budget before Blynk processes its login
reply. Each network operation retains its own core/transport timeout; the outer
deadline is checked between Blynk runs and cannot interrupt an operation.
SNTP runs in the background; Blynk TLS waits for a valid clock
without blocking startup. Missing internet/NTP/Blynk does not prevent local sensing
or home-network OTA. This is cooperative scheduling, not hard real-time control;
serial diagnostics report the maximum actual sample gap each minute.

## Configure modern Blynk

Cloud setup requires your signed-in Blynk account. No account, template, device, or
dashboard has been created by this source change. The legacy app/token cannot be
reused. Create a template named **Smoke Detector**, hardware **ESP8266**, connection
**Wi-Fi**, then a device from that template. Put its template ID and new device
token in your local `Secrets.h`.

Create these virtual-pin datastreams. Use zero as the default for numeric values,
unless noted. Epoch values are UTC Unix seconds, not formatted local time. String
datastreams have no numeric range.

| Pin | Name | Type / range | Dashboard / cadence |
| --- | --- | --- | --- |
| V0 | Test command | Integer 0–1 | Momentary PUSH button |
| V1 | Alarm active | Integer 0–1 | Red status indicator; on change |
| V2 | Motion per 15 minutes | Integer 0–100000 | History chart; every complete online bucket |
| V3 | Wi-Fi RSSI | Integer -127–0 | Value in dBm; hourly and on connection |
| V4 | Uptime seconds | Double 0–1000000000000 | Value; hourly and on connection |
| V5 | Free heap | Integer 0–100000 | Bytes; hourly and on connection |
| V6 | Last movement UTC | Double 0–4294967295 | Timestamp value; hourly and on connection |
| V7 | Test result | String | Status label; on change and connection |
| V8 | Last successful test UTC | Double 0–4294967295 | Timestamp value; on success and connection if known |
| V9 | Wi-Fi IP address | String | Value; on connection and IP change |
| V10 | Restart reason | String | Value; on connection |
| V11 | Prepare silence | Integer 0–1 | Momentary PUSH button |
| V12 | Confirm silence | Integer 0–1 | Separate momentary PUSH button |
| V13 | Silence result | String | Status label; on change and connection |
| V14 | Outage summary | String | Diagnostic label/history; after outages or overflow |
| V15 | Enable updates | Integer 0–1 | Momentary PUSH button; never sync commands |
| V16 | Update mode status | String | Value/status label; on connection and requests |

V14 is an additional diagnostic stream for outage length, offline motion counts,
and dropped queue records, so these reports do not replace V7/V13 control results.
Set all four command buttons to PUSH, not SWITCH; react only to value 1. Disable
**sync latest value on device connection** for V0, V11, V12 and V15. Do not add app
automations that repeat these commands. Firmware never requests command syncing.
Where available, make telemetry datastreams read-only for dashboard users.

V9 now shows, for example, `192.168.1.123`. Reuse the existing
String datastream and value widget; rename their label to **Wi-Fi IP address** in
the web and iOS dashboards. The IP is the device's local Wi-Fi address for
OTA uploads. Offline, the widget retains the last reported address, which
may be stale until the next connection. Wi-Fi signal strength remains on V3.
The firmware version remains in the serial startup log.

Enable saved history for V2 and use a column/bar chart of 15-minute counts with
raw values or sums over non-overlapping intervals. Do not interpret the counts
as number of people. Build the web and mobile dashboards separately: alarm and
controls at the top, V7/V8/V13 alongside the controls, then the motion chart and
health values. The app's available widgets and history retention depend on your
account. If a timestamp-formatting widget is unavailable, label the values as
Unix UTC seconds. Set dashboard/account timezone to Europe/Riga when supported.

Create these custom events in the template:

| Event code | Push notification | Timeline |
| --- | --- | --- |
| `smoke_alarm` | Enabled, your user selected | Enabled |
| `smoke_clear` | Disabled | Enabled |
| `test_failed` | Enabled, your user selected | Enabled |

Leave event-counter suppression and additional notification suppression periods
disabled for these events. Firmware spaces all custom events at least six seconds apart and caps
attempts at 90 in a sliding 24-hour window. Remaining events stay queued. The
counter is RAM-only and resets with the device. Keep server-side limits enabled.
See [Blynk event limits](https://docs.blynk.io/en/blynk.console/limits).

In **Template → Connection Lifecycle**, set the Online-to-Offline wait period to
five minutes; enable your user's offline push notification and disable control
widgets while offline. Keep **Log when device reports any data** off for this
continuously connected device, so hourly health reporting is not mistaken for an
offline device. Heartbeat is 60 seconds. Verify the actual notification timing
after both a graceful disconnect and loss of power/internet. See
[Connection Lifecycle](https://docs.blynk.io/en/blynk.console/templates/connection-lifecycle).

### Test and silence

**Test:** while online, idle, and not alarming, press Test once. The detector
button is held LOW for two seconds. Buzzer detection within ten seconds reports
**Buzzer response detected**; otherwise **No buzzer response detected** and a
`test_failed` event. Overlapping tests are rejected. A detected episode still active
at 15 seconds from test start is promoted to a normal smoke alarm notification.
The V1 indicator remains truthful throughout testing. This checks the
button/buzzer/input path, not the detector's smoke sensitivity. A real alarm
coinciding with an intentional test cannot be distinguished by this hardware.

**Silence:** during an active alarm, press Prepare silence, then Confirm silence
within ten seconds. GPIO5 is held LOW for one second, matching the old sketch.
Attempts are separated by a 30-second cooldown. V13 shows **Silence requested**,
then **Alarm cleared** on observed clearance, or **Alarm still detected** after
15 seconds. The latter can subsequently change to **Alarm cleared**. An alarm
that starts again generates a new alarm event. Silence never clears the alarm
state, notification queue, or sensing by itself. Disconnect/restart, clearance,
or timeout cancels pending confirmation. Tests, updates, and concurrent button
operations reject silence requests. Rejected commands do not drive the button;
the reason is recorded on serial, while current alarm/control status stays visible.

### History, outages, and budget

- Motion totals are published once per 15-minute bucket, including zero. Buckets
  are relative to device startup. Startup, offline, and partially offline buckets
  are omitted; the first normal chart point is usually about 30 minutes after boot.
- Offline motion is summarized in V14, never inserted into a current chart bucket.
  Summary counts cover accumulation since the preceding successfully submitted
  summary; they are not lifetime totals.
  Last movement is RAM-only and V6 returns to zero after restart. Zero also means
  the movement occurred before clock synchronization.
- Test result resets to **Not tested since restart**. V8 is not overwritten with
  zero after restart, retaining the previous successful timestamp in Blynk. A new
  successful test supplies a new timestamp.
- Sixteen alarm/test event records fit in RAM. Alarm starts evict lower-priority
  records when full; if all entries are starts, the oldest is replaced. Overflow
  is summarized in V14. Delayed events include their age, whether they occurred
  offline, and current alarm status. Blynk timestamps their eventual delivery.
- `logEvent()` has no application-level delivery acknowledgement. Immediate
  connection failures are retried, but delivery remains best effort and retries
  can duplicate events. A device power loss discards RAM queues and outage totals.
- In a 31-day month: 2,976 motion writes + 2,976 hourly health writes = **5,952
  routine writes**, plus snapshots, commands, state changes and events. The boot
  gap normally removes one motion write. Blynk internal pings do not consume the
  message-limit quota, although traffic counters can show them; see
  [Blynk engineering clarification](https://community.blynk.cc/t/blynk-message-limit/73735/16).
  Confirm actual usage in your account's **monthly message quota**, not just the
  sent/received network counter. See [message accounting](https://docs.blynk.io/en/message-usage).

## Build and first installation

Pinned dependencies: **Arduino CLI 1.3.1**, **ESP8266 core 3.1.2**, **Blynk 1.3.5**.
TLS uses Blynk's current `certs/certs_pem.h` CA bundle with hostname and certificate
validity verification retained. Firmware version: **2.0.3**.
`sketch.yaml` selects NodeMCU 1.0 (`nodemcuv2`) with 4 MB flash / 1 MB filesystem.
This is a build default, not a claim that the actual installed board has 4 MB.
No filesystem is used by this firmware.

1. Install [Arduino CLI 1.3.1](https://github.com/arduino/arduino-cli/releases/tag/v1.3.1)
   and put it on PATH, or pass its executable path using `-ArduinoCli`.
2. Copy `Secrets.example.h` to `Secrets.h`. Fill in your new Blynk credentials,
   Wi-Fi settings and a unique OTA password of at least 12 characters. This file
   is ignored by Git. Replace any still-active keys exposed in the old source.
3. From this project in PowerShell:

   ```powershell
   .\tools\build.ps1
   ```

   The script stages source/credentials in `$env:TEMP\smoke-detector-build`, uses
   isolated dependency caches and downloads the pinned packages on the first run.
   `-WorkRoot` changes that location. The staged source and binaries contain
   secrets: keep that directory private. `-CheckOnly` compiles with placeholder
   credentials in a separate `check` subdirectory; **never upload its output**.
4. During physical access, record the module markings and check the actual flash
   size with the serial flashing tool before selecting a layout. If it is not
   4 MB, correct the `eesz` option in `sketch.yaml` before installing. Validate
   available flash can hold running firmware and the update image together.
5. Upload the real build using the actual serial port (example COM5):

   ```powershell
   arduino-cli upload --config-file "$env:TEMP\smoke-detector-build\arduino-cli.yaml" --profile nodemcu --port COM5 --input-dir "$env:TEMP\smoke-detector-build\output" "$env:TEMP\smoke-detector-build\SmokeDetector"
   ```

6. Reset the board after the first serial upload. At 115200 baud, check reported
   actual/configured flash, sketch size, free update space and **OTA eligible**.
   A size mismatch or insufficient free space disables OTA without stopping
   monitoring. The serial log prints the Wi-Fi IP on connection and address
   changes, and the OTA listener's start/stop status. V9 also reports the IP.
7. Complete the installation checklist below before closing the enclosure.

Arduino IDE is also supported: install the same ESP8266 and Blynk versions,
open `SmokeDetector.ino`, use the NodeMCU board and verified flash layout, then
compile/upload. The CLI profile does not automatically configure the IDE.

## Local OTA and recovery

OTA works on networks that permit communication between the Mac and board and
does not require Blynk connectivity or SNTP. Firmware **2.0.4** also includes a
direct update mode for networks where that communication fails:

1. While the detector is idle, press **Enable updates** (V15) in Blynk. Alternatively,
   hold the NodeMCU **FLASH** button for three seconds after normal startup, then
   release it. Holding FLASH during reset enters the chip's serial bootloader;
   press it only after the firmware has started. It must be released once after
   startup before physical activation is armed.
2. Connect the Mac to **smoke-detector-direct** using the existing
   `SMOKE_OTA_PASSWORD` from your private `Secrets.h`. The hotspot admits one client
   and the board is at **192.168.4.1**.
3. Open the normal sketch with the same credentials and OTA support, select
   **NodeMCU 1.0**, **4MB (FS:1MB OTA:~1019KB)** and the **smoke-detector at
   192.168.4.1** network port in Arduino IDE, then upload with the OTA password.
4. Reconnect the Mac to its usual Wi-Fi. After a successful update the board
   reboots into normal mode; update mode is never persisted across restarts.

The hotspot closes after **five minutes** and the board reconnects to its usual
Wi-Fi. An already accepted upload may finish beyond the deadline. Blynk is
temporarily offline in this AP-only mode; local sensor processing continues until
the actual firmware transfer. An alarm or detector operation before transfer
cancels update mode and restores normal networking. The Blynk V16 acknowledgement
may remain visible while offline; reconnection refreshes it. V15 resets to zero,
and repeated presses do not extend the window. Keep USB accessible for recovery.
The temporary `/private/tmp` diagnostic helper is not needed for routine updates.
The activity LED on GPIO13 blinks rapidly while the hotspot is active
(if that external LED is connected).

The mobile and web dashboards use the same datastreams but have separate layouts.
In **Blynk.App → Developer Mode → Smoke Detector**, add a **Button** on **Enable
updates (V15)**, set its mode to **PUSH**, OFF/ON values to **0/1**, and label it
**Enable updates**. Add a **Value Display** on **Update mode status (V16)**.
The web dashboard already has these controls. Command synchronization on reconnect
must remain disabled; firmware never requests command synchronization.

The listener is closed while an alarm, test, or button operation is active;
pending transfers are discarded too. GPIO5 is released before an accepted update.
Sensor servicing pauses during upload/reboot and briefly during OTA admission.
The original detector must continue sounding independently of the ESP.

Build the next firmware with the same secrets, compatible layout and OTA support.
Increment `kFirmwareVersion` in `Settings.h`. Use either:

- Arduino IDE's network port for `smoke-detector`, entering the OTA password.
- The pinned core's `tools/espota.py`, supplying the device IP when mDNS fails:

  ```powershell
  python '<ESP8266-core-3.1.2-path>\tools\espota.py' --ip 192.168.1.123 --auth '<OTA-password>' --file "$env:TEMP\smoke-detector-build\output\SmokeDetector.ino.bin" --progress
  ```

  The core path is in the build's data directory; locate `espota.py` there if
  using isolated CLI profiles. Keep verbose upload logs private because upload
  tools can expose their password argument. Firewall rules must permit the
  device's OTA UDP port 8266 and the connection back to the upload computer.

On macOS, browse for about 20 seconds while the board is idle:

```bash
dns-sd -B _arduino._tcp local.
# Press Ctrl-C after observing the results.
```

If discovery fails, use the IP printed on serial or V9 with `espota.py`.
Check Arduino IDE's Local Network access in macOS System Settings and ensure
both devices are on a network that permits communication between clients.

Check the device reconnects and V9 reports its IP; check the new firmware version
in the serial startup log. Perform another OTA
upload to prove the new firmware retained update support. Authentication/update
failures resume monitoring. An interrupted upload should leave the old image
usable if the final flash-copy phase has not begun; a power failure during copying
or a valid but broken new image can require serial recovery. There is **no
automatic application rollback**. Do not expose OTA ports to the internet.

For recovery, reconnect serial, select the verified board/flash layout, and upload
a known-good build containing the correct credentials and OTA routines. See
[ESP8266 OTA documentation](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html).

## Tests and acceptance

Run the hardware-independent C++ tests with a host compiler:

```powershell
.\tools\test.ps1 -Compiler g++
# Alternatively, with a local Zig installation:
.\tools\test.ps1 -Compiler '<path-to-zig.exe>' -Zig
```

These test ADC hysteresis/beep gaps, alarm transitions and recurrence, successful
and failed tests, test escalation, button timing, silence expiry/cooldown,
disconnect/restart cancellation, queue priority/overflow/retry order, event rate
limits, PIR warm-up, outage buckets, timer rollover, backoff, OTA admission and a
simulated 31-day message budget. They cannot validate electrical behavior or cloud
delivery. The full ESP8266 build verifies actual library integration.

Installation acceptance gates (do not claim success until physically checked):

- [ ] A0 voltage and idle/buzzer levels verified; thresholds discriminate real
  beep gaps and noise. Minute diagnostics show acceptable sample gaps under TLS.
- [ ] GPIO5 stays released during boot, reconnect, rejected commands and updates;
  two-second test and one-second silence operate the real button correctly.
- [ ] Normal boot with the existing GPIO15 LED wiring; PIR warm-up and rising-edge
  counts behave correctly.
- [ ] Successful test, failed-response case, persistent test alarm escalation and
  overlapping-command rejection verified.
- [ ] Prepare/confirm silence, ten-second expiry, 30-second cooldown, ongoing
  alarm display and renewed alarm notification verified.
- [ ] Boot without Wi-Fi and with invalid Blynk credentials: local monitoring
  continues, retries stay paced, and local OTA works once Wi-Fi is available.
- [ ] Reconnect/restart does not replay commands. Actual phone notifications,
  history values, offline timing and delayed-event ages verified.
- [ ] Wrong OTA password and interrupted upload verified; updates rejected during
  alarms/tests; two consecutive valid updates complete before enclosure closure.
- [ ] 24-hour soak: no unexpected resets, heap does not continually fall, sample
  gaps remain acceptable and monthly message usage matches the expected cadence.

The retrofit is supplementary telemetry/control. Guided testing and cloud
notifications do not establish that the underlying detector still meets its
manufacturer's smoke-detection performance requirements.
