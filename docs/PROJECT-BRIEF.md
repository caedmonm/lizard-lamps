# ESP32 Dual Heat Lamp Controller

## Project overview

This project is an ESP32-based temperature control system designed to manage two 230V heat lamps, initially for a reptile enclosure.

The ESP32 monitors two independent temperature sensors and controls the output of a two-channel AC dimmer. Rather than simply switching heating on and off, it adjusts lamp power proportionally according to how far the measured temperatures fall below their configured targets.

**The ultimate goal is a standalone, Wi-Fi-connected heating controller with a web interface, REST API and programmable heating schedules.**

The device should continue operating autonomously even if Wi-Fi or an external server becomes unavailable.

## 1. Hardware

### Main components

- ESP32-WROOM-32 development board
- Two DS18B20 digital temperature probes
- OneWire bus with 4.7kΩ pull-up
- Two-channel AC TRIAC dimmer with zero-cross detection
- Two 230V heat lamps (approximately 150W each)
- USB power supply for the ESP32
- Suitable protective enclosure, mains protection and independent over-temperature safety cutout

### ESP32 connections

| Component | Signal | ESP32 GPIO |
|---|---|---|
| AC dimmer | VDD | 3V3 |
| AC dimmer | GND | GND |
| AC dimmer | Z-C | GPIO23 |
| AC dimmer | D1 | GPIO25 |
| AC dimmer | D2 | GPIO26 |
| DS18B20 sensors | OneWire data | GPIO4 |
| DS18B20 sensors | Power | 3V3 |
| DS18B20 sensors | Ground | GND |

Both DS18B20 sensors currently share the same OneWire bus.

The dimmer independently controls the two lamps using phase-angle dimming. Mains wiring is separate from the ESP32 logic circuitry.

## 2. Existing firmware

The project is currently an Arduino sketch called `heat-lamp-control.ino`, targeting the ESP32 using Arduino Core 3.x.

### Libraries

- `OneWire`
- `DallasTemperature`
- `MycilaDimmer` (`MycilaDimmers.h`)
- `MycilaPulseAnalyzer`

The Mycila libraries provide TRIAC phase-angle control with zero-cross synchronisation.

The mains frequency is 50Hz, with a half-cycle duration of 10,000 microseconds.

### Current functionality

The firmware:

1. Initializes the two DS18B20 temperature sensors.
2. Initializes both AC dimmer channels.
3. Reads both temperatures approximately once per second using asynchronous sensor conversion.
4. Calculates the required heating power based on two independently configurable target temperatures.
5. Applies that power to one selected lamp, leaving the other switched off.
6. Logs temperatures, targets, selected lamp and output power to the Serial Monitor.
7. Commands the lamps off if sensor readings are invalid or mains zero-cross synchronisation is lost.

The existing sketch is the starting point and should be preserved and refactored rather than replaced unnecessarily.

## 3. Temperature control algorithm

The controller maintains two independent target temperatures.

Initial configuration:

```cpp
float targetTemperatures[2] = {
  15.0,
  15.0
};

int activeLamp = 0;

const float FADE_TEMP_RANGE = 3.0;
```

`activeLamp` is zero-indexed:

- `0` = lamp 1
- `1` = lamp 2

Only one lamp is active at a time.

### Heating behaviour

Either temperature sensor can request heating.

Each sensor's temperature deficit is calculated:

```cpp
float deficit1 = targetTemperatures[0] - temperatures[0];
float deficit2 = targetTemperatures[1] - temperatures[1];

float deficit = max(deficit1, deficit2);
```

The required output power is then calculated:

```cpp
float power = constrain(
  deficit / FADE_TEMP_RANGE,
  0.0f,
  1.0f
);
```

This produces proportional control over a 3°C temperature range.

| Greatest temperature deficit | Requested power |
|---|---|
| 0°C or less | 0% |
| 0.5°C | 17% |
| 1°C | 33% |
| 1.5°C | 50% |
| 2°C | 67% |
| 2.5°C | 83% |
| 3°C or greater | 100% |

The greater temperature deficit always determines the heating demand.

For example, if sensor 1 is exactly at its target but sensor 2 is 2°C below its target, the selected lamp receives approximately 67% power.

The other lamp remains completely off.

This is proportional temperature-based control, not a fixed-duration fade animation. Lamp power changes as measured temperature changes.

### Important design detail

The two sensors do not correspond exclusively to the two lamps. Both sensors influence the currently selected lamp.

The active lamp will eventually be configurable through the schedule or API.

## 4. Future functionality

### A. Wi-Fi connectivity

The ESP32 should connect to the local Wi-Fi network.

It should:

- Support configurable Wi-Fi credentials.
- Automatically reconnect after disconnection.
- Synchronize its clock using NTP.
- Continue heating control without an active network connection.
- Recover cleanly after power interruption.

A captive-portal-style Wi-Fi setup could be added later.

### B. REST API

Ultimately, we want to configure and monitor the system through an API.

A lightweight HTTP server running directly on the ESP32 is a sensible initial architecture, although a separate backend could be introduced later.

Suggested API:

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/status` | GET | Current temperatures, targets, lamp power and status |
| `/api/config` | GET | Retrieve controller configuration |
| `/api/config` | PUT | Update target temperatures, fade range and settings |
| `/api/schedule` | GET | Retrieve the heating schedule |
| `/api/schedule` | PUT | Update the heating schedule |
| `/api/override` | POST | Temporarily override scheduled settings |

Example status response:

```json
{
  "temperatures": [14.2, 16.5],
  "targets": [15, 17],
  "activeLamp": 0,
  "lampPower": [27, 0],
  "fadeRange": 3,
  "mode": "scheduled",
  "wifiConnected": true,
  "sensorsHealthy": true,
  "dimmerOnline": true
}
```

Configuration changes should take effect without rebooting.

All API inputs must be validated, particularly target temperatures, selected lamp and output settings.

Configuration and schedules should be persisted in ESP32 non-volatile storage using Preferences/NVS or LittleFS.

### C. Heating schedules

The system should support different temperature targets at different times of day.

For example:

| Time | Target 1 | Target 2 | Active lamp |
|---|---|---|---|
| 07:00 | 25°C | 22°C | 0 |
| 12:00 | 28°C | 25°C | 0 |
| 17:00 | 24°C | 22°C | 1 |
| 21:00 | 15°C | 15°C | 1 |

The scheduler should support:

- Multiple schedule entries per day.
- Independent target temperatures for both sensors.
- Selecting which lamp is active during each period.
- Different schedules for different days of the week.
- Explicit disabled/heating-off periods.
- Manual overrides, ideally with an expiry time.

The system should use a configurable time zone, initially `Europe/London`, and correctly handle daylight saving changes.

Schedule execution should happen locally on the ESP32 so that internet access is not required during normal operation.

After a reboot, the system should restore its saved configuration and determine the appropriate scheduled state once valid time is available.

### D. Web interface

Eventually, the ESP32 should provide a browser-accessible management interface.

A responsive UI, usable on mobile and desktop, should include:

**Dashboard**

- Current temperatures from both probes.
- Target temperatures.
- Selected lamp.
- Current output power for both channels.
- System status, connectivity and any sensor faults.

**Schedule editor**

- Visual daily/weekly schedule.
- Editable temperature setpoints.
- Selection of active lamp for each period.
- Ability to add and remove schedule entries.

**Manual controls**

- Temporarily adjust target temperatures.
- Switch the active lamp.
- Disable heating.
- Return to scheduled operation.

**Settings**

- Fade temperature range.
- Wi-Fi configuration.
- Time zone.
- Sensor identification and calibration.
- Safety limits and system information.

The UI should communicate with the ESP32 through the REST API.

A lightweight frontend hosted directly by the ESP32 may be sufficient initially. We can decide later whether an external backend or remote dashboard is necessary.

## 5. Firmware architecture

As the application grows, the firmware should be divided into logical modules rather than keeping everything in one large Arduino sketch.

Suggested structure:

```text
heat-lamp-control/
├── heat-lamp-control.ino
├── config.h
├── TemperatureManager.h/.cpp
├── DimmerController.h/.cpp
├── HeatingController.h/.cpp
├── ScheduleManager.h/.cpp
├── ConfigManager.h/.cpp
├── WiFiManager.h/.cpp
├── ApiServer.h/.cpp
└── web/
```

Suggested responsibilities:

**TemperatureManager:** Discover and read both DS18B20 sensors, verify sensor health, and associate each physical sensor with a fixed hardware address.

**DimmerController:** Manage MycilaDimmer, zero-cross detection and the two AC outputs. Provide a simple interface such as `setPower(lamp, percent)`.

**HeatingController:** Implement the proportional temperature algorithm. Accept temperatures and target temperatures, calculate requested heating power and select the output lamp.

**ScheduleManager:** Determine the active temperature targets and lamp selection according to the current schedule.

**ConfigManager:** Load, validate and persist configuration.

**WiFiManager / ApiServer:** Handle network connectivity, API requests and eventually the web frontend.

The temperature-control calculation should be separated from hardware-specific code so it can be unit-tested independently.

## 6. Safety and reliability

This controls real heating equipment and must fail safely.

Requirements:

- Both lamps should default to OFF during startup until the controller is initialized and valid temperature readings are available.
- If either temperature sensor becomes unavailable or reports invalid data, command both lamps OFF.
- Detect stale sensor readings, not just explicitly disconnected sensors.
- Handle zero-cross detection failures by commanding the dimmer outputs OFF where possible.
- Ensure only the selected lamp can receive power.
- Validate all settings received from the API.
- Prevent unintended heating when the schedule or time is unavailable.
- Restore configuration safely after reboot.
- Avoid blocking operations in the main control loop.
- Ensure network requests and web operations cannot interfere with heating control.
- Use an independent, mains-rated hardware over-temperature cutout, because software cannot protect against a TRIAC failing short-circuit.

Also consider adding software maximum-temperature limits, logging faults and providing clear warnings through the web interface.

Mains installation, enclosure, earthing, protection and thermal suitability must be independently verified. The firmware is not a substitute for hardware safety protection.

## 7. Development priorities

Implement this in stages.

**Phase 1 — Core controller**

Keep the existing hardware and working MycilaDimmer functionality. Refactor the code into logical modules, maintaining both temperature readings, independent targets, proportional heating and active lamp selection.

Identify temperature sensors by their unique OneWire addresses rather than relying on index order.

**Phase 2 — Configuration and persistence**

Move the hardcoded target temperatures, active lamp and fade range into a configuration structure with persistent storage.

**Phase 3 — Wi-Fi and API**

Add network connectivity and a REST API for retrieving live status and changing settings.

**Phase 4 — Scheduling**

Add a local weekly scheduler, persistent schedule configuration and temporary overrides.

**Phase 5 — Web interface**

Build a responsive UI for monitoring temperatures, editing the schedule and changing settings.

**Phase 6 — Improvements**

Consider temperature history, graphs, OTA firmware updates, configurable alarms and additional diagnostics.

## 8. Guidance for Codex

Please inspect the existing Arduino sketch and preserve the working hardware integration.

The immediate priority is to establish a clean, maintainable foundation for the temperature controller, with clear separation between temperature measurement, heating calculations and dimmer hardware.

Keep the implementation reasonably lightweight and suitable for an ESP32.

Do not introduce unnecessary external services or complex frameworks at this stage.

The API, scheduling system and web UI are longer-term requirements, but the firmware architecture should accommodate them from the outset.

Where hardware details or scheduling behaviours are ambiguous, identify the assumptions rather than inventing requirements.

The end result should be a dependable, standalone heating controller that can eventually be fully configured and monitored through a browser or external API.
