# Validation

## Software checks

The firmware compiles with `esp32:esp32:esp32`, Espressif core 3.3.12, OneWire 2.3.8, DallasTemperature 4.0.6, MycilaDimmer 3.0.1 and MycilaPulseAnalyzer 3.0.10.

History/control build: 1,176,873 bytes of flash (89% of the default app partition); global/static RAM 111,308 bytes (33%), leaving 216,372 bytes for heap and stacks before runtime allocations. The history ring is below 58 KB, enforced at compile time. History JSON streams in roughly 1 KB chunks without a ring snapshot or a full-response allocation. The 1.375 MiB default filesystem partition holds at most 71,932 bytes of log/header payload plus LittleFS overhead. These compile-time figures are not a runtime heap measurement.

Run `./tests/run.sh` with a C++17 compiler:

- Pure control tests cover exact switch boundaries, midnight wrap, distinct start times, all minutes across representative schedules, active day/night targets at switching boundaries, full demand through small target deficits, both-target shutdown, restart hysteresis boundaries, demand from either probe, restart after faults, period/target changes, invalid/non-finite/over-limit readings and strict form value validation. Minimum-floor tests cover cold startup without scheduled demand, basking-only floor demand, ambient independence with retained fault shutdown, exact 5°C day-assistance threshold and immediate release above it, night-only holding boundaries, repeat dips, normal-demand resumption, invalid minimum/readings and hardware fault shutdown/recovery. Output fade tests cover the default 1-second ramp, disabled fading and custom 10-second ramps, frequent updates, partial demand, sequential lamp changes, reversals, immediate fault shutdown and millisecond rollover.
- History tests cover minute boundaries, 2,881-sample capacity, strict 48-hour expiry, backwards clock correction, temperature encoding, output percentages, all four targets, stale/missing readings and over-limit values. File tests restore a full 48-hour log after simulated reboot, check fixed storage size and skip corrupt/truncated records. Persistence integration tests exercise actual save orchestration with mocked flash: fresh/unknown filesystem handling, output-off guarding, failed-write retries, probe swaps, invalid clocks and maintenance timeout. History API tests cover authentication, empty/data JSON and no-store headers. History client tests cover range changes, navigation, all eight series, per-reading inspection, line gaps, empty state and retry errors.
- Web tests compile the actual `DeviceWeb.cpp` against mocked ESP32 peripherals. They exercise provisioning, authentication checks, CSRF rejection, invalid or missing day/night targets, invalid schedules, persistence failure handling, saved state, probe swapping, escaped network names and reset confirmation, authenticated status/configuration/connection JSON, API write validation and null/escaped JSON values. They render all six page types to HTML.

Network tests exercise the actual web networking code with controlled time: exact 60-second retry boundaries, repeated failures, recovery hotspot preservation, successful reconnection, another outage, immediate credential changes, millis rollover and no station attempts without saved credentials. These mocks do not establish successful real-world router reconnection.

Configuration tests exercise the actual storage code against mocked NVS: v1/v2/v3/v4 layout migration to v5 with a 1-second fade default, persisted fade/restart/minimum settings, range validation, credentials/probe preservation, day/night persistence, partial setup, invalid configuration and a failed migration write.

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

The desktop browser preview was checked with simulated 48-hour, one-minute data, including all eight series, percentage/temperature axes, series toggling, Earlier navigation, reading inspection and the configurable fade field. These checks have not been performed on hardware for the history feature.

## Hardware acceptance checklist

Record board/dimmer/lamp models, wiring arrangement, probe addresses, core/library versions and test conditions. Use suitable independent protection and qualified mains installation.

- First boot: both outputs off, unique open hotspot requiring no password, setup works on desktop and mobile. Verify fallback at 192.168.4.1 if a captive browser does not open.
- Correct/incorrect/open-network Wi-Fi credentials; router loss and return must recover without reboot. Check explicit retries every 60 seconds, recovery hotspot availability during repeated attempts and closure two minutes after reconnection. Disconnect only the router WAN and verify LAN/dashboard/heating remain available; restore WAN and check network-time synchronization. Check connection status, lizard-lamps.local discovery and assigned IP fallback; hotspot closes after successful connection.
- Dashboard favicon, sign-in, navigation and mobile layout; overview and connection progress update through AJAX without navigation/reloads; form validation preserves edits, successful writes update the interface, and interrupted connections show stale-reading feedback.
- Distinct day/night targets and schedule survive reboot; upgrades preserve credentials/probes/times and copy the previous targets into both periods; overview is the default after configuration.
- Heating control: fade settings and restart drop survive reboot; test the 1-second default and custom durations, as well as disabled fading. Verify full demand until both targets, timed fade-out only after reaching both, and restart at the exact configured drop on either probe. Verify fault recovery and period/settings changes reset on/off state. Confirm actual lamp behavior and flicker with the installed load.
- Correct probe identity and label swapping; disconnect either probe and confirm output off; reconnect and verify recovery.
- Exact start-time switching of both lamp and targets, midnight wrap and configured fade-out, 20ms transition gap and configured fade-in in both directions; confirm no overlap at the physical outputs.
- NTP and browser clock fallback; cold boot without a clock allows minimum-temperature protection while scheduled heating waits; network loss after valid time preserves local scheduling.
- Minimum-temperature protection: set and reboot to verify persistence; cold boot with router/internet unavailable and basking below the floor must power the night lamp after valid readings/zero-cross detection. Test basking just above, exactly at and below minimum − 5°C: day assistance must run only at or below that threshold and stop immediately above it. Vary healthy ambient readings independently and confirm minimum demand is unchanged. Warm basking to the floor and confirm night-only holding between the minimum and minimum + 0.5°C with no scheduled demand. Cool basking below the floor again, check night-only heating and the day-assistance threshold, then restore scheduled demand and verify normal targets resume. Repeat through settings saves, zero-cross interruptions and probe faults; faults/reset/maintenance must stop both outputs immediately. Verify both simultaneous loads and thermal response on hardware.
- London spring/autumn transitions follow documented wall-clock behavior.
- Zero-cross loss, over-temperature readings, partial initialization and reset/startup output behavior.
- Slow HTTP clients, repeated form requests, NVS writes and Wi-Fi reconnection do not interrupt healthy control or delay fault shutdown.
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
- Historical proportional dimming could round a small deficit to zero; the current controller requests full demand for any deficit when initially enabled, keeps full demand until both targets, and then uses restart hysteresis. Normal transitions use the configured fade and a 20ms settling gap.
- Wi-Fi loss alone is absent from the heating enable conditions. It should not stop heating after a valid clock has been obtained. A blocked/crashed control task, electrical fault or failed load remains outside what the temperature guard can repair.

These are code findings, not reproduction of the reported hardware incident.
