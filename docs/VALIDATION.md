# Validation

## Software checks

The firmware compiles with `esp32:esp32:esp32`, Espressif core 3.3.12, OneWire 2.3.8, DallasTemperature 4.0.6, MycilaDimmer 3.0.1 and MycilaPulseAnalyzer 3.0.10.

Run `./tests/run.sh` with a C++17 compiler:

- Pure control tests cover exact switch boundaries, midnight wrap, distinct start times, all minutes across representative schedules, active day/night targets at switching boundaries, adjustable proportional demand, full-on/full-off hysteresis boundaries, demand from either probe, state resets on faults/period/target/mode changes, invalid/non-finite/over-limit readings and strict form value validation.
- Web tests compile the actual `DeviceWeb.cpp` against mocked ESP32 peripherals. They exercise provisioning, authentication checks, CSRF rejection, invalid or missing day/night targets, invalid schedules, persistence failure handling, saved state, probe swapping, escaped network names and reset confirmation, authenticated status/configuration/connection JSON, API write validation and null/escaped JSON values. They render all six page types to HTML.

Configuration tests exercise the actual storage code against mocked NVS: v1/v2 layout migration, persisted dimming/restart settings, range validation, credentials/probe preservation, day/night persistence, partial setup, invalid configuration and a failed migration write.

Client tests (when Node.js is available) execute the actual embedded JavaScript against a DOM/network mock, covering live readings, active period changes, faults, dimming field visibility, heating-control form saves, form errors and Wi-Fi connection progress. Python validates the generated JSON responses.

The mock tests do not simulate Wi-Fi, flash writes, hardware timing, browser Digest authentication negotiation or execution of the separate FreeRTOS tasks. Rendered HTML is not proof of captive-portal or browser compatibility.

To generate sample HTML pages with simulated conditions:

```sh
mkdir -p /tmp/lizard-preview
c++ -std=c++17 -Itests/stubs tests/web_test.cpp -o /tmp/lizard-web-test
/tmp/lizard-web-test /tmp/lizard-preview
```

These pages contain simulated readings and are previews only. Form submissions require the real controller.

## Hardware acceptance checklist

Record board/dimmer/lamp models, wiring arrangement, probe addresses, core/library versions and test conditions. Use suitable independent protection and qualified mains installation.

- First boot: both outputs off, unique open hotspot requiring no password, setup works on desktop and mobile. Verify fallback at 192.168.4.1 if a captive browser does not open.
- Correct/incorrect/open-network Wi-Fi credentials; connection status, lizard-lamps.local discovery and assigned IP fallback; hotspot closes after successful connection.
- Dashboard sign-in, navigation and mobile layout; overview and connection progress update through AJAX without navigation/reloads; form validation preserves edits, successful writes update the interface, and interrupted connections show stale-reading feedback.
- Distinct day/night targets and schedule survive reboot; upgrades preserve credentials/probes/times and copy the previous targets into both periods; overview is the default after configuration.
- Heating control: both modes and ranges survive reboot; dimming tracks the configured range; with dimming off, verify only 0%/100% commands, shutdown when both probes reach their targets and restart at the exact configured drop on either probe. Verify fault recovery and period/settings changes reset on/off state. Confirm actual lamp behavior and flicker with the installed load.
- Correct probe identity and label swapping; disconnect either probe and confirm output off; reconnect and verify recovery.
- Exact start-time switching of both lamp and targets, midnight wrap and 20ms transition gap in both directions; confirm no overlap at the physical outputs.
- NTP and browser clock fallback; cold boot without a clock keeps outputs off; network loss after valid time preserves local scheduling.
- London spring/autumn transitions follow documented wall-clock behavior.
- Zero-cross loss, over-temperature readings, partial initialization and reset/startup output behavior.
- Slow HTTP clients, repeated form requests, NVS writes and Wi-Fi reconnection do not interrupt healthy control or delay fault shutdown.
- Dashboard access: verify protected and unprotected first-time setup, disable/re-enable/change password, protection state after restart, sign-in prompts and CSRF rejection in both modes. Existing saved passwords must remain enabled after upgrade.
- Factory reset physically commands outputs off, clears all saved application/station data and reopens the password-free setup hotspot.
- Independent cutout operation and behavior under a failed-short TRIAC are assessed independently of firmware.

No physical hardware acceptance results are claimed by this repository yet.
