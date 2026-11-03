# Validation

## Software checks

The firmware compiles with `esp32:esp32:esp32`, Espressif core 3.3.12, OneWire 2.3.8, DallasTemperature 4.0.6, MycilaDimmer 3.0.1 and MycilaPulseAnalyzer 3.0.10.

Browser OTA/history build: 1,190,997 bytes of flash (90% of the default app partition); global/static RAM 111,708 bytes (34%), leaving 215,972 bytes for heap and stacks before runtime allocations. The history ring is below 58 KB, enforced at compile time. History JSON streams in roughly 1 KB chunks without a ring snapshot or a full-response allocation. The 1.375 MiB default filesystem partition holds at most 71,932 bytes of log/header payload plus LittleFS overhead. These compile-time figures are not a runtime heap measurement.

Run `./tests/run.sh` with a C++17 compiler:

- Pure control tests cover exact switch boundaries, midnight wrap, distinct start times, all minutes across representative schedules, active day/night targets, PI demand at small deficits from either probe, switching the controlling probe, partial power at the last target, response to warming/cooling, full-power and fade anti-windup, both-target relative cutoff, fault/period/target resets, fresh-conversion timing, skipped samples, history pauses, millis rollover, invalid/non-finite/over-limit readings and strict tuning validation. Minimum-floor tests cover cold startup without scheduled demand, basking-only floor demand, ambient independence with retained fault shutdown, exact 5°C day-assistance threshold and immediate release above it, night-only holding boundaries, repeat dips, normal-demand resumption, invalid minimum/readings and hardware fault shutdown/recovery. Output fade tests cover the default 1-second ramp, disabled fading and custom 10-second ramps, frequent updates, partial demand, sequential lamp changes, reversals, immediate fault shutdown and millisecond rollover.
- The real sketch loop is also compiled against mocked dimmers, probes and clock. Tests exercise actual PI/fader/floor routing, immediate fault/ceiling shutdown, stale samples, history pauses, day/night switching without overlap, and floor recovery without a clock. Delayed first-order thermal simulations use 0.0625°C probe quantization, the actual fader, and a 2°C room-temperature drop. They check partial-power holding near the last target with either probe controlling; these simplified models are regression tests, not evidence of installed hardware performance.
- History tests cover minute boundaries, 2,881-sample capacity, strict 48-hour expiry, backwards clock correction, temperature encoding, output percentages, all four targets, stale/missing readings and over-limit values. File tests restore a full 48-hour log after simulated reboot, check fixed storage size and skip corrupt/truncated records. Persistence integration tests exercise actual save orchestration with mocked flash: fresh/unknown filesystem handling, output-off guarding, failed-write retries, probe swaps, invalid clocks and maintenance timeout. History API tests cover authentication, empty/data JSON and no-store headers. History client tests cover range changes, navigation, all six series, per-reading inspection, line gaps, empty state and retry errors.
- OTA tests exercise the real upload handlers and maintenance guard with mocked flash: authentication/CSRF rejection before writes, missing OTA partition, output-off timeout, truncation, invalid image magic, write/validation failures, multiple files, aborted/timed-out/oversized bodies, parser cleanup and success holding outputs off through reboot. Browser upload tests check headers, file selection, progress, restart, server errors and uncertain connection results. Active-target history tests cover day/night transitions, minimum protection, legacy gaps and persistence across reloads without changing record size.
- Web tests compile the actual `DeviceWeb.cpp` against mocked ESP32 peripherals. They exercise provisioning, authentication checks, CSRF rejection, invalid or missing day/night targets, invalid schedules, persistence failure handling, saved state, probe swapping, escaped network names and reset confirmation, authenticated status/configuration/connection JSON, API write validation and null/escaped JSON values. They render all six page types to HTML. Mocked socket tests cover failed headers, failed streamed payloads, partial writes on sockets that still report connected, slow-response budget expiry across millis rollover, a single library write overrunning the budget, and a successful full-history/status request from the next client.

Network tests exercise the actual web networking code with controlled time: exact 60-second retry boundaries, repeated failures, recovery hotspot preservation, successful reconnection, another outage, immediate credential changes, millis rollover and no station attempts without saved credentials. These mocks do not establish successful real-world router reconnection. A source review of Arduino-ESP32 3.3.12 found that WebServer ignores socket write results while NetworkClient can retry a stalled write without closing the socket. The application now closes failed/over-budget responses and stops history streaming. This is a plausible failure path, not a reproduction or confirmed explanation of the reported 15-minute outage on 2 November 2026.

Configuration tests exercise the actual storage code against mocked NVS: v1–v5 layout migration to v6 with default PI tuning, preserved v5 fades and 1-second fades for earlier versions, persisted fade/PI/minimum settings, range validation, credentials/probe preservation, day/night persistence, partial setup, invalid configuration and a failed migration write.

Client tests (when Node.js is available) execute the actual embedded JavaScript against a DOM/network mock, covering live readings, active period changes, faults, dimming field visibility, heating-control form saves, form errors and Wi-Fi connection progress. Python validates the generated JSON responses.

The mock tests do not simulate Wi-Fi, flash writes, hardware timing, browser Digest authentication negotiation or execution of the separate FreeRTOS tasks. Rendered HTML is not proof of captive-portal or browser compatibility.

To generate sample HTML pages with simulated conditions:

```sh
mkdir -p /tmp/lizard-preview
c++ -std=c++17 -Itests/stubs tests/web_test.cpp -o /tmp/lizard-web-test
/tmp/lizard-web-test /tmp/lizard-preview
```

These pages contain simulated readings and are previews only. Form submissions require the real controller.

## History acceptance checks

- Leave the board running for over 48 hours with the browser closed; verify one-minute spacing and automatic expiry. Check runtime free heap while repeatedly opening the overview and fetching history.
- Unplug one probe: its series should show gaps while the other continues; reconnect it and verify recovery.
- Test touch/keyboard range navigation, resizing and connection loss/recovery in desktop and mobile browsers.
- Power-cycle the board and verify committed records return after clock synchronization, with genuine downtime gaps. Cut power during a save and confirm older records remain readable. Swap probe labels and confirm history clears. Verify a clock/storage failure is shown in the chart. Check flash-save pauses do not restart the fade and assess their physical effect on the lamps.

The desktop browser preview was checked with simulated 48-hour, one-minute data, including all six series, percentage/temperature axes, series toggling, Earlier navigation, reading inspection and the configurable fade field. The PI settings screen and fade toggle were also checked in a desktop browser using rendered test fixtures. These checks have not been performed on hardware for the history feature.

## Hardware acceptance checklist

Record board/dimmer/lamp models, wiring arrangement, probe addresses, core/library versions and test conditions. Use suitable independent protection and qualified mains installation.

- First boot: both outputs off, unique open hotspot requiring no password, setup works on desktop and mobile. Verify fallback at 192.168.4.1 if a captive browser does not open.
- Correct/incorrect/open-network Wi-Fi credentials; router loss and return must recover without reboot. Check explicit retries every 60 seconds, recovery hotspot availability during repeated attempts and closure two minutes after reconnection. Disconnect only the router WAN and verify LAN/dashboard/heating remain available; restore WAN and check network-time synchronization. Check connection status, lizard-lamps.local discovery and assigned IP fallback; hotspot closes after successful connection.
- Dashboard favicon, sign-in, navigation and mobile layout; overview and connection progress update through AJAX without navigation/reloads; form validation preserves edits, successful writes update the interface, and interrupted connections show stale-reading feedback.
- Distinct day/night targets and schedule survive reboot; upgrades preserve credentials/probes/times and copy the previous targets into both periods; overview is the default after configuration.
- Heating control: verify PI tuning and fade settings survive reboot and v5 upgrades retain Wi-Fi/password/probes/targets. Verify 31.5°C against a 32°C target requests partial heat even with ambient above target; reverse which probe is below target and verify it controls demand. Monitor cold start and steady holding on each lamp, then a room-temperature change; verify settling and tune proportional band/holding adjustment for this enclosure. Check the temperature of the probe that reaches its target first, especially with an unattainable second target, and verify either probe's fault/45°C cutoff stops heating. Confirm partial-power compatibility/flicker with the installed loads, including slow fades and fading disabled. Verify immediate zero output when both probes reach their target + 1°C, including a long fade, and confirm physical overshoot remains acceptable. Check period/target changes and fault recovery discard learned power, floor recovery cannot wind it up, and history pauses retain holding power.
- Correct probe identity and label swapping; disconnect either probe and confirm output off; reconnect and verify recovery.
- Exact start-time switching of both lamp and targets, midnight wrap and configured fade-out, 20ms transition gap and configured fade-in in both directions; confirm no overlap at the physical outputs.
- NTP and browser clock fallback; cold boot without a clock allows minimum-temperature protection while scheduled heating waits; network loss after valid time preserves local scheduling.
- Minimum-temperature protection: set and reboot to verify persistence; cold boot with router/internet unavailable and basking below the floor must power the night lamp after valid readings/zero-cross detection. Test basking just above, exactly at and below minimum − 5°C: day assistance must run only at or below that threshold and stop immediately above it. Vary healthy ambient readings independently and confirm minimum demand is unchanged. Warm basking to the floor and confirm night-only holding between the minimum and minimum + 0.5°C with no scheduled demand. Cool basking below the floor again, check night-only heating and the day-assistance threshold, then restore scheduled demand and verify normal targets resume. Repeat through settings saves, zero-cross interruptions and probe faults; faults/reset/maintenance must stop both outputs immediately. Verify both simultaneous loads and thermal response on hardware.
- London spring/autumn transitions follow documented wall-clock behavior.
- Zero-cross loss, over-temperature readings, partial initialization and reset/startup output behavior.
- Slow HTTP clients, repeated form requests, NVS writes and Wi-Fi reconnection do not interrupt healthy control or delay fault shutdown. During a full 48-hour history download, interrupt the client connection or stop it reading; verify the server closes the failed response, logs the abort, and serves the next client while Wi-Fi recovery continues. Measure actual socket blocking time; the response budget is checked between library writes, not enforced inside a write.
- Dashboard access: verify protected and unprotected first-time setup, disable/re-enable/change password, protection state after restart, sign-in prompts and CSRF rejection in both modes. Existing saved passwords must remain enabled after upgrade.
- Factory reset physically commands outputs off, clears all saved application/station data and reopens the password-free setup hotspot.
- Independent cutout operation and behavior under a failed-short TRIAC are assessed independently of firmware.

No physical hardware acceptance results are claimed by this repository yet.

## Heating-off investigation

Code review identifies these paths in the prior firmware; there is no stored fault history to establish which occurred on 24 October 2026:

- A power interruption/reboot discards wall time. Without NTP (router, DNS or internet failure) or browser time, both lamps stayed off indefinitely even with healthy cold probes. The minimum guard now covers this case.
- On/off mode waits for its restart drop, including after a reboot, fault, settings write or period change. A reading below target but inside that band can legitimately leave both outputs off. Floor protection now overrides the band if the basking probe falls below the minimum.
- Either invalid/disconnected/stale or over-limit probe shuts down both outputs, regardless of how cold the other probe is. Missing/invalid probe bindings or invalid saved configuration also prevent scheduled control. Probe faults must remain shutdown conditions.
- Unavailable zero-cross detection stops both outputs. Partial dimmer initialization is only attempted during startup; an initialization failure requires reboot. A lost zero-cross signal after successful initialization should recover when valid pulses resume.
- Configuration writes stop heating while flash is written and release maintenance on success or failure. A factory reset that fails to erase storage intentionally remains stopped until retried or rebooted.
- Historical proportional dimming could round a small deficit to zero; the intervening on/off controller used a 2°C restart band. The current PI controller removes that band, follows the largest probe deficit, and learns holding power so small persistent errors do not remain at zero output. Normal transitions use the configured fade and a 20ms settling gap.
- Wi-Fi loss alone is absent from the heating enable conditions. It should not stop heating after a valid clock has been obtained. A blocked/crashed control task, electrical fault or failed load remains outside what the temperature guard can repair.

These are code findings, not reproduction of the reported hardware incident.

## Browser OTA hardware checks (pending)

- Install the OTA-capable build over USB with the default OTA partition layout. On the LAN, open Device → Firmware update and upload the application `.bin`. Verify progress, restart and repeat uploads to exercise both slots.
- Measure both dimmer outputs during flashing, validation, reboot, interrupted uploads and rejected images. Verify minimum protection pauses while flashing, control resumes after failure, and the ordinary startup checks gate heating after reboot.
- Check dashboard Digest negotiation in actual browsers, CSRF rejection, wrong-chip/invalid/oversized images, unplugged Wi-Fi, upload timeout and USB recovery. Verify settings and saved history survive successful updates. A booting but broken application has no automatic rollback in this implementation.
- Observe a day/night transition on the graph: one target per probe, correct historical values after schedule edits, and only a basking floor target during minimum protection. Legacy readings must retain temperatures/outputs with unavailable active targets.
