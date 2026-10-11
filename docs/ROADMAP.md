# Roadmap

The supplied sketch is the working starting point. Preserve its hardware integration while developing a lightweight, maintainable controller. The full requirements and examples are in [the original briefing](PROJECT-BRIEF.md).

## Phase 1 — Core controller

- Separate temperature measurement, proportional heating calculations and dimmer hardware into logical modules.
- Bind each probe to its unique OneWire address.
- Maintain independent targets, greatest-deficit control and one selected lamp.
- Add stale-reading detection, explicit fault handling and independently testable control calculations.
- Verify startup, partial initialization, sensor faults and zero-cross loss with outputs commanded off.

## Phase 2 — Configuration and persistence

- Create a validated configuration structure for targets, active lamp and fade range.
- Save configuration in Preferences/NVS or LittleFS.
- Define safe defaults and handling for missing, invalid or corrupted settings.

## Phase 3 — Wi-Fi and API

- Add configurable Wi-Fi credentials, reconnection and NTP time synchronization.
- Keep heating control independent of networking and HTTP request handling.
- Implement validated status/configuration APIs with changes applied without rebooting.

Proposed endpoints, not yet available:

| Method | Path | Purpose |
| --- | --- | --- |
| GET | `/api/status` | Temperatures, targets, lamp power and health |
| GET / PUT | `/api/config` | Read/update controller configuration |
| GET / PUT | `/api/schedule` | Read/update schedules |
| POST | `/api/override` | Temporary override |

## Phase 4 — Local scheduling

- Persist weekly schedules with multiple daily entries, independent targets and active lamp selection.
- Support explicit heating-off periods and expiring manual overrides.
- Support a configurable time zone, initially Europe/London, including daylight saving changes.
- Restore configuration after reboot and resolve the current period once valid time is available.
- Define behavior when time/schedule is unavailable, avoiding unintended heating.

## Phase 5 — Web interface

- Responsive live dashboard with temperatures, targets, selected lamp, output and faults.
- Daily/weekly schedule editor, manual override controls and settings.
- Lightweight frontend served by the ESP32 and communicating through its API.

## Phase 6 — Improvements

Consider temperature history, graphs, OTA updates, configurable alarms and diagnostics.

## Decisions still to make

- Exact dimmer model, lamp compatibility and independently validated safety hardware.
- Physical probe mapping, calibration and installation-specific temperature bounds.
- Safe time-unavailable policy, clock holdover and daylight-saving transition semantics.
- Schedule interpolation versus immediate setpoint changes, midnight rollover and override precedence.
- API authentication, provisioning and local-network access policy.
- Persistence format, fault recovery and watchdog strategy.

These are open design questions; the project does not yet define their behavior.
