# Roadmap

The controller now hosts its own interface on the ESP32. The earlier proposal for a REST API has been dropped. No cloud or separate backend is planned.

## Implemented prototype

- First-boot password-protected hotspot, captive DNS and Wi-Fi provisioning.
- Persistent credentials, settings and fixed OneWire probe identities.
- Responsive on-device overview, target editor, daily lamp schedule and device/reset pages.
- Independent basking/ambient targets and greatest-deficit proportional heating.
- Local daily schedule, midnight wrap, London daylight saving and clock fallback.
- Wi-Fi recovery hotspot and authenticated, CSRF-protected web forms.
- Stale/invalid/over-limit temperature shutdown and zero-cross fault shutdown.
- Separate web task and hardware control loop, with output-off acknowledgment before configuration writes or reset.
- Native control/web-handler tests and ESP32 compilation.

## Next priority — Hardware validation

Run the [validation checklist](VALIDATION.md) on the actual board, dimmer and probes. Confirm output behavior during startup, reset, lamp transitions, sensor faults, time loss and network/flash operations. Validate thermal/electrical protection independently.

## Possible later improvements

- Multiple daily lamp-switch entries or a weekly schedule.
- Configurable timezone, schedule-dependent targets and explicit heating-off periods.
- Temperature history, visual graphs and alarms.
- Latched faults, watchdog/recovery policy and additional diagnostics.
- OTA firmware updates with clear output-off behavior.
- Probe replacement/pairing without resetting all settings.
- Installer-friendly printed hotspot credentials and dashboard password changes.

The current product uses two daily start times and constant per-probe targets. These choices keep the initial interface simple; richer scheduling is a future option.
