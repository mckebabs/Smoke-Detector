# Implementation validation

## macOS bench continuation (3 October 2026)

- Firmware **2.0.3** adds local Wi-Fi IP reporting to the existing V9 String
  stream. The first build reported `2.0.3 | IP 192.168.1.173` in Blynk.
  The final display was simplified to the IP alone so it fits the existing
  widget, following the user's suggestion to reuse V9. Firmware version remains
  in serial logs. The web widget is labeled **Wi-Fi IP address**. The final IP-only
  value was received in Blynk. At the tested web-panel width, the last digit was
  clipped; widening that widget remains outstanding. Browser resize attempts
  did not change its width. The phone layout has not been inspected.
  The existing iOS V9 widget receives the same value; its presentation has not
  been inspected on the phone.
- Serial now logs Wi-Fi connection/address changes, OTA listener requests/stops,
  and minute reports with the IP and OTA eligibility/listener flags.
- Full target compile passed with the installed Arduino CLI **1.5.1**, ESP8266
  core **3.1.2**, Blynk **1.3.5**, and `nodemcuv2:eesz=4M1M`. CLI 1.3.1 was
  not installed on this Mac. Final static RAM: **33,144 / 80,192 bytes (41%)**;
  instruction RAM: **62,139 / 65,536 bytes (94%)**; flash code:
  **413,148 / 1,048,576 bytes (39%)**; binary: **451,920 bytes**.
- Host tests: **9 scenario groups, 274 assertions passed**, using Apple Clang
  with warnings treated as errors.
- USB flash identification confirmed **4 MB**. The real-credential diagnostics
  firmware was uploaded over USB with the written image hash verified. Private
  staged source and binaries remain outside Git in a restricted temporary folder.
- The board's MAC matched the Mac's ARP entry for **192.168.1.173**. Blynk
  reported it online. The Mac received no responses to three pings, a UDP OTA
  authentication-challenge probe, or mDNS discovery. Local Network access was
  already enabled for Arduino IDE and ChatGPT; both board and Mac use the same SSID.
  At this stage the cause of the missing OTA port remained unresolved, and
  no OTA uploads had yet been confirmed. Subsequent direct-network tests below
  completed the two-consecutive-OTA acceptance gate.
- After the final USB upload, serial reported `WiFi 7` (disconnected), `Blynk 0`,
  and `OTA eligible/listening 1/0`, with 100 ms steady sampling and about 34 KB
  free heap. The image is eligible, but the listener is stopped while Wi-Fi is
  unavailable. Wi-Fi reconnection and a physical-reset startup capture remain
  outstanding. Blynk retains the previously reported IP while disconnected.
- Following the final IP-only USB upload, serial confirmed `WiFi 3`, `Blynk 1`,
  and `OTA eligible/listening 1/1`, with **192.168.1.173**, approximately 13 KB
  heap and a 965 ms maximum sample gap in the first report. Blynk received
  the IP-only V9 value. A new ping and UDP OTA probe still received no replies
  while the listener was active. The earlier disconnect therefore does not
  explain all discovery/reachability failures. Blynk's latest RSSI was -87 dBm.
  Further diagnostics focus on the Mac, as requested by the user.
- After a physical reset, serial again confirmed **192.168.1.173**, Wi-Fi and
  Blynk connected, and OTA eligibility/listener flags **1/1**. Steady sample
  gaps remained around 100 ms. A Mac packet capture showed repeated outgoing
  ARP requests for that address with no received replies. A temporary ARP entry
  using the board's USB-verified MAC **60:01:94:17:81:92** also produced outgoing
  unicast ICMP packets without replies. Two direct UDP 8266 probes similarly
  left `en0` for the verified MAC, with no response within two seconds each.
  The temporary entry was removed afterward. This confirms a local reachability
  failure beyond mDNS discovery; it does not establish whether packets fail
  within the network or at the board. Client isolation remains a hypothesis.
- macOS firewall was disabled during these checks. The GitHub CLI account was
  authenticated, but the repository API reported no push permission. Changes
  have not been published to GitHub.
- Blynk MCP was configured locally and OAuth login succeeded. The current
  chat initially returned **Authentication required**; web dashboard verification
  used the signed-in Blynk console instead. On **2026-10-04**, MCP searches and
  device/template reads succeeded. V9 (datastream ID 10) was renamed from
  **Firmware version** to **WiFi IP address**, retaining its String type, pin,
  units and history/automation flags. A device read verified the new name and
  **192.168.1.173** value. The web label shows the full IP with the device list
  collapsed; narrow layouts can clip it. Phone presentation remains uninspected.
- On **2026-10-03/04**, the user authorized a temporary direct Wi-Fi diagnostic.
  A private build outside Git created **smoke-detector-direct** at
  **192.168.4.1**, protected by the existing OTA password, limited to one client
  and expiring after 20 minutes. Mac network switching restored the usual Wi-Fi
  afterward without printing credentials. Direct ping and an OTA password
  challenge succeeded, and mDNS advertised **smoke-detector**. Arduino CLI
  explicitly listed the network port **smoke-detector at 192.168.4.1**.
- The initial AP+station diagnostic allowed one authenticated OTA upload, but
  a second upload and a later retry failed around reconnects. The final diagnostic
  used AP-only mode to remove station channel changes from that test, with local
  detector processing retained and Blynk temporarily offline. The uploader waited
  for a fresh OTA challenge after reboot before sending each image.
- On **2026-10-04**, **two consecutive password-authenticated OTA uploads passed**:
  the AP-only diagnostic image (**453,360 bytes**, **17.0 seconds**) followed by
  normal firmware 2.0.3 (**451,920 bytes**, **17.6 seconds**). Both received the
  device's successful image result. The second upload removed the diagnostic AP.
  Mac Wi-Fi was restored; USB serial then confirmed **WiFi 3**, **Blynk 1**,
  **192.168.1.173**, OTA flags **1/1**, **12,920 bytes** free heap, no alarm, and
  a **100 ms** maximum sample gap. Blynk MCP independently reported the device
  Online and received V9 with the normal Wi-Fi IP after reboot.
  Discovery and OTA therefore work on the Mac via a direct board connection.
  The home-network communication path remains unresolved; these results do not
  identify a specific router setting or prove client isolation.
- On **2026-10-04**, the user reran the terminal helper: USB hash verification
  passed, followed by two authenticated OTA uploads (**18.0** and **19.6 seconds**).
  The helper reported normal firmware and Mac Wi-Fi restored. The user then
  loaded the same diagnostic with `--ide-setup` and confirmed that uploading
  normal firmware through **Arduino IDE** also worked. These are direct-network
  results; the temporary AP is not included in the repository's normal firmware.

## Five-minute maintenance mode (4 October 2026)

- Normal firmware **2.0.4** includes the protected, one-client
  **smoke-detector-direct** hotspot at **192.168.4.1**. Blynk **V15** requests it;
  **V16** reports status. Holding NodeMCU FLASH after startup for three seconds
  is the offline fallback. Activation is deferred out of cloud callbacks, refused
  when the detector cannot admit OTA, and never persisted across reboot. Alarms
  before transfer cancel the window. An accepted transfer can finish across the
  five-minute deadline. Repeated requests do not extend the window.
- Host checks passed **10 scenario groups / 307 assertions**, including admission
  with alarm/test state, five-minute expiry, pending cancellation, timer rollover,
  release-before-arming and one activation per physical hold.
- ESP8266 3.1.2/Blynk 1.3.5 target compile passed with warnings enabled. Final
  binary **454,336 bytes**; static RAM **33,324 / 80,192**, instruction RAM
  **62,139 / 65,536**, flash code **415,400 / 1,048,576**. The initial build was
  USB-flashed with the written image hash verified.
- Blynk MCP created the V15 Integer 0–1 command and V16 String status without
  history or automation access. The web dashboard has a **PUSH** button and status
  label; Save And Apply confirmed changes applied to one device. The V15 server
  synchronization setting was inspected and remained disabled.
- A real Blynk-triggered window expired after five minutes. The board returned to
  normal Wi-Fi, Blynk Online and **192.168.1.173**, reset V15 to zero, and published
  the expiry status. Steady AP-mode sample gaps were **100 ms**; transitions and
  TLS reconnection produced longer gaps, so real detector validation remains open.
- The user held FLASH for three seconds. The direct network and OTA listener
  responded, and an authenticated OTA upload of the final normal firmware passed
  in **17.8 seconds**. Mac Wi-Fi was restored; Blynk independently reported Online,
  V15 zero and V16 **Ready for updates** after reboot.
- The saved web **Enable updates** button was then pressed on the final firmware.
  Serial confirmed hotspot admission, **192.168.4.1** and `direct updates 1`.
  A second authenticated OTA upload passed in **17.9 seconds**, followed by
  **OTA complete; rebooting** and Mac Wi-Fi restoration. This confirms update-mode
  activation and OTA support are retained after uploading the normal firmware.
  After the final reboot, serial confirmed **WiFi 3**, **Blynk 1**, OTA flags
  **1/1**, **direct updates 0**, no alarm and about **12.7 KB** free heap. Blynk
  independently confirmed a fresh connection, V15 zero, V16 **Ready for updates**
  and V9 **192.168.1.173**. Dashboard proof is saved as
  `blynk-update-mode-verified.png` in the chat's visualization directory.
- Blynk.Console's **App Dashboard** page requires editing in Blynk.App on the
  phone. The shared datastreams are available, but the mobile Button/Value Display
  still need to be added there; this is documented in README. Physical LED wiring,
  alarm-driven hotspot cancellation, interrupted-update recovery and the real
  detector installation checks remain unverified on this bare bench board.

## Real detector test-button timing

- The user connected the actual detector and observed LED activity without an
  audible test response, followed by Blynk **No buzzer response detected**.
  The original sketch at `ab57c4b` and firmware 2.0.4 both held the button for
  **2,000 ms**. A manual detector-button test sounded after approximately
  **three seconds**, with about two beeps after the user released the button.
- Firmware **2.0.5** holds the test button for **at most five seconds** and
  releases it immediately on the first detected buzzer response. The ten-second
  response deadline and fifteen-second escalation remain measured from test
  start. The one-second silence press is unchanged. This targets the timing
  mismatch. The user subsequently confirmed the real app-triggered audible
  test worked; Blynk reported **Buzzer response detected** and recorded a
  successful test at **2026-10-05 00:07 Europe/Riga**.
- Host checks passed **10 scenario groups / 321 assertions**, including a
  three-second buzzer response followed by two beeps, immediate release,
  five-second release without a response, response timeout, alarm escalation
  and rollover. The target compile passed with warnings enabled; static RAM
  **33,324 / 80,192**, instruction RAM **62,139 / 65,536**, flash code
  **415,416 / 1,048,576**.
- The **454,352-byte** binary was installed through authenticated direct OTA in
  **12.2 seconds**. Mac Wi-Fi was restored, and Blynk independently confirmed a
  fresh connection, no alarm, **Not tested since restart** and **Ready for updates**.
  The agent did not send a test or silence command. Real detector hush behavior
  remains unverified.

## Readable date/time fields

- Firmware **2.0.6** publishes V6 and V8 as `YYYY-MM-DD HH:MM` strings in
  **Europe/Riga** time, automatically following winter/summer time. Internal
  timestamps remain UTC. V6 reports **Not recorded since restart** when no
  clock-qualified movement is known; V8 retains the last successful value in
  Blynk across restart.
- Both existing Blynk datastreams were changed from Double to String without
  replacing their IDs/pins. Their existing epochs were converted and preserved:
  V6 **2026-10-05 00:12**, V8 **2026-10-05 00:07**. Web widget titles identify
  Riga time. Mobile widgets should remain bound to V6/V8 and use Value Display.
- Host checks passed **11 scenario groups / 330 assertions**, including the
  recorded successful test, winter/summer offsets and both DST transitions.
  The NodeMCU target compile passed: static RAM **34,188 / 80,192**, instruction
  RAM **62,139 / 65,536**, flash code **425,736 / 1,048,576**;
  binary **465,488 bytes**.
- After the user reconnected the detector, the **465,488-byte** 2.0.6 image
  was installed through authenticated direct OTA in **12.1 seconds**, and the
  Mac's usual Wi-Fi was restored. Image SHA256:
  `60c6e0da046aee07fde8f6dbb62edb61f3845ce60a817f666226e2e403701b9e`.
  Blynk confirmed a fresh online connection at **2026-10-04 21:22:22 UTC**,
  no active alarm and **Ready for updates**. V6 now publishes the expected
  startup string **Not recorded since restart** rather than numeric zero;
  V8 retained **2026-10-05 00:07**. No test or silence command was sent.

The earlier records below describe the Windows implementation validation.

Verified locally with Arduino CLI 1.3.1, ESP8266 core 3.1.2, Blynk 1.3.5 and
the NodeMCU `eesz=4M1M` build target. The compile used example credentials in a
separate staging directory. The user has uploaded firmware to a bare ESP8266;
detector wiring and installation acceptance have not yet been validated.

## Completed

- Host C++ tests: **9 scenario groups, 274 assertions passed**, compiled with
  warnings treated as errors. Includes a simulated 31-day routine-message budget.
- Full ESP8266 compile and link: **passed**, with `--warnings all`.
- Build memory report: static RAM **33,060 / 80,192 bytes (41%)**; instruction RAM
  **62,139 / 65,536 bytes (94%)**, including 32,768 bytes reserved for instruction
  cache; flash code **412,628 / 1,048,576 bytes (39%)**.
- Firmware 2.0.2 binary: **451,328 bytes**, built with example credentials only;
  do not upload this validation binary.
- Blynk 1.3.5 integration: protocol timeout updated to the supported 1,000 ms
  minimum; separate DNS/TCP/socket timeout remains 300 ms. Current Blynk CA
  bundle is loaded with hostname and validity verification retained.
- Connection budget corrected from 1 second to 20 seconds. The Blynk library
  counts TLS setup inside that deadline; the old deadline could expire after
  sending login but before consuming the server response. Normal Blynk serial
  logs plus failed-attempt duration/TLS error/token-invalid status and minute
  Wi-Fi/clock/cloud indicators are enabled. No token or password is logged.
- No IFTTT/ThingSpeak clients, legacy notification API, blocking application
  delays, command synchronization, or insecure TLS mode remain in the firmware.

Target validation used the installed ESP8266 core 3.1.2 under Arduino15 and the
user's installed Blynk 1.3.5 library, with a staged sketch lacking the profile
file and an explicit FQBN/library path. The normal build script uses the updated
pinned profile. Host tests were rerun after the compatibility changes.

## Outstanding installation checks

The user reports that the web/iOS dashboards have been configured. Cloud delivery,
history widgets and monthly quota usage have not been verified by these code checks.
An earlier bare-board report showed zero telemetry writes despite a cloud Online badge,
and approximately 1.8-second maximum sample gaps. Subsequent user-provided 2.0.2
logs showed successful Blynk connection, telemetry and 100–101 ms steady sample
gaps. The Mac's USB bench reports also showed Blynk connected, 100 ms sample gaps
and approximately 13 KB free heap. Startup/reconnect sample gaps still require
hardware validation. Compilation and model
tests do not prove actual TLS timing or cloud delivery.

Flash capacity is confirmed as 4 MB. A0 voltage and thresholds, PNP button behavior, LED
boot behavior, PIR behavior, offline sensing, phone notifications, OTA
interruption recovery and the 24-hour soak require the
physical-access session. Complete the README installation checklist before
closing the device. The host tests and target build do not replace these checks.
