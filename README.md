# Lizard Lamps — ESP32 Dual Heat Lamp Controller

A self-contained ESP32 controller for two AC heat lamps, originally built for a reptile enclosure. It hosts its own modern, responsive web dashboard: connect to its setup hotspot, enter Wi-Fi details, then manage your habitat from the device's IP address.

**The web dashboard and its JSON API run directly on the ESP32.** No cloud service, frontend build or external server is required. All CSS, JavaScript and the dashboard favicon are embedded in firmware.

## What it does

- Open Wi-Fi setup hotspot (no password) with a captive-portal DNS redirect.
- Local dashboard showing **basking spot** and **ambient** temperatures, targets, selected lamp, requested power, connectivity and faults.
- Two daily start times for switching between lamp 1 and lamp 2, including periods spanning midnight.
- Independent day and night basking/ambient targets, with adjustable dimming or full-on/full-off heating on the selected lamp.
- Configurable minimum-temperature protection: both lamps recover a cold enclosure; the night lamp maintains the floor without Wi-Fi or wall time.
- Persistent Wi-Fi details, dashboard password, targets, schedule, heating control settings and fixed probe addresses.
- Probe label swapping, Wi-Fi recovery after disconnection and a confirmed factory reset.
- Europe/London clock with daylight saving, NTP synchronization and browser-time fallback.

**Status: implemented prototype.** Firmware compilation and native control/web-handler tests pass; physical operation, captive-portal behavior and electrical/thermal safety still require hardware validation. See [validation](docs/VALIDATION.md) and [safety notes](docs/SAFETY.md).

> This project controls 230V mains heating equipment. Use suitable enclosure/protection and an independent, mains-rated over-temperature cutout. A TRIAC can fail short-circuit and leave a lamp on despite software commands.

## First-time setup

1. Build and upload the firmware, with both DS18B20 probes connected.
2. On your phone or computer, join the **LizardLamps-XXXXXX** Wi-Fi hotspot. **No hotspot password is needed.**
3. Open a browser and visit **[http://192.168.4.1](http://192.168.4.1)**. The setup page may also open automatically. Stay connected if your phone warns that the hotspot has no internet.
4. Enter the name and password of your **2.4GHz** Wi-Fi network and choose whether to require a dashboard password. Protection is enabled by default; use 8–63 characters, or uncheck **Require a password to manage this device**. Empty Wi-Fi passwords are supported for open networks.
5. If protection is enabled, sign in with username **admin** and your chosen dashboard password.
6. Open **Schedule & temps** and save both periods together: **Day · Lamp 1** and **Night · Lamp 2** each have a start time, basking spot target and ambient target. Initial values are 19°C for all four targets, day at 07:00 and night at 19:00; save the combined form before scheduled heating can run. Minimum-temperature protection defaults to 15°C; choose your required floor in **Settings → Heating control**.
7. Rejoin your normal Wi-Fi and visit **[http://lizard-lamps.local](http://lizard-lamps.local)**. If your network does not resolve that name, use the LAN IP shown on the connection success page, dashboard banner or Device page. Serial Monitor at 115200 baud also prints both addresses. The setup hotspot closes two minutes after a successful connection.

Once the combined schedule and targets are saved, the default page is the live overview. It polls the status API approximately every 2 seconds and updates readings, targets, selected lamp, power and faults in place. Wi-Fi connection progress also updates through AJAX. Settings forms save through AJAX with inline confirmation/errors; editing does not reload the page. The interface works without downloading fonts, scripts or styles from the internet.

Identify your physical probes by their readings and displayed OneWire addresses. Use **Settings → Swap probe labels** if the basking and ambient labels are reversed. Probe addresses are discovered once when exactly two probes are attached, then saved; a missing probe is never silently replaced. To bind replacement probes, factory reset, attach both new probes and restart.

## Where to open the dashboard

| Your phone/computer is connected to | Browser address |
| --- | --- |
| LizardLamps-XXXXXX setup/recovery hotspot | [http://192.168.4.1](http://192.168.4.1) |
| Your normal home Wi-Fi | [http://lizard-lamps.local](http://lizard-lamps.local), or the device LAN IP shown after setup |

192.168.4.1 is the hotspot address; the controller gets a separate address from your home router. If protection is enabled, sign in with username **admin** and your chosen dashboard password.

## Schedule and temperature control

The schedule repeats daily in **Europe/London** local time. Lamp 1 runs from its start time until lamp 2's start time; lamp 2 runs until lamp 1 starts again. Start times must differ. For example, 07:00 / 19:00 selects lamp 1 during the day and lamp 2 overnight. Reversing those times makes lamp 1 cover the overnight period.

During normal scheduled heating, only the selected lamp receives a nonzero power command. Power changes fade linearly over 10 seconds. On a lamp change, the outgoing lamp fades to off over 10 seconds, both outputs stay off for a 20ms settling period, then the incoming lamp fades to its requested power over 10 seconds. A complete change between powered lamps takes approximately 20 seconds; normal scheduled transitions never overlap. Minimum-temperature recovery intentionally powers both lamps. New power requests during a fade start a new 10-second ramp from the current output. Each period has its own basking and ambient targets; switching periods changes the selected lamp and both target temperatures together. The overview shows the active period’s targets.

Both probes influence the selected lamp:

```text
largest deficit = max(active period basking target - basking temperature,
                      active period ambient target - ambient temperature)
with dimming on:
requested power = clamp(largest deficit / dimming range, 0, 1) × 100%
```

Open **Settings → Heating control** to turn dimming on or off. With dimming on (the default), set the **Dimming range**: the lamp receives full power when the greatest deficit reaches this range and progressively dims as the targets approach. The default range is 3°C.

With dimming off, the selected lamp targets either 100% or 0%, with a 10-second fade between them. It stays on until **both** probes reach their respective targets, then stays off until **either** probe falls by the **Restart temperature drop** below its target. For example, a 30°C target and a 2°C drop restart heating at 28°C. The default drop is 2°C. Both ranges accept 0.1–10°C and apply to both day and night. Each mode retains its own range when switching modes.

On startup, after a fault or settings save, and when the active period or its targets change, on/off control starts from off and waits for the restart threshold. Existing sensor, clock and zero-cross safeguards apply in both modes.

### Minimum safe temperature

**Settings → Heating control → Minimum safe temperature** accepts 5–40°C and is saved across reboots. The default, including upgrades from v1–v3, is **15°C**; choose a value appropriate to your enclosure.

If **either** healthy probe reads below the minimum, **both lamps immediately request 100%**, bypassing normal fades and the schedule. Recovery continues until **both** probes reach the minimum. When normal scheduled heating has no demand (including its on/off restart band), or a clock/schedule is unavailable, the **night lamp alone** maintains the floor: on at the minimum, off when both probes reach minimum + 0.5°C. Any new reading below the minimum reactivates both lamps. Normal scheduled heating resumes whenever it requests heat after recovery; the floor never lowers its targets.

This guard operates locally without Wi-Fi, NTP or a browser, after the first healthy readings and valid zero-cross detection, including before schedule setup. Missing/invalid/stale probes, either probe reaching 45°C, unavailable zero-cross detection, maintenance and reset still command both lamps off. A stopped processor, absent mains supply or failed lamp cannot be corrected by this software.

The overview labels **Minimum recovery · Both lamps** and **Minimum hold · Night lamp**, and displays the minimum as the active target during floor protection. Requested power is the greater of the two channel requests, not their sum; the status API also reports each channel separately.

The sketch rounds to a whole percentage and enables Mycila's power lookup table. With dimming on and the default 3°C range, a 2°C greatest deficit requests approximately 67% power. Requested power is not measured electrical wattage.

Targets are validated between **5°C and 40°C**. If either measured temperature reaches the prototype's fixed **45°C** limit, is invalid/disconnected, or becomes stale, heating is commanded off immediately, bypassing fades. Maintenance and factory reset also stop heating immediately. These bounds are firmware safeguards, not recommended husbandry temperatures or certified safety protection.

Scheduled heating is disabled until the combined schedule and all four targets are saved and a valid clock is available. Minimum-temperature protection remains available without that schedule or clock. After time has been set, scheduling continues locally without Wi-Fi. After a power interruption, scheduled heating waits for NTP; minimum-temperature protection runs in the meantime. To restore scheduling, wait for NTP or press **Use browser time** on the overview; check the phone/computer clock first. JavaScript also supplies fallback time when saving setup/settings forms.

DST behavior is based on the current wall-clock period: spring skips nonexistent local times; the autumn repeated hour follows the repeated local times. The controller selects the appropriate lamp from the current time, rather than replaying missed events.

## Upgrading existing devices

Saved settings from configuration versions 1–3 are migrated automatically. The minimum defaults to 15°C; existing Wi-Fi credentials, dashboard password, switch times, probe mappings and targets are preserved. Version 3 retains its chosen dimming mode and ranges. Versions 1–2 default to dimming on with a 3°C range and a 2°C restart drop. Version 1's shared basking/ambient targets are copied into both periods. Edit **Schedule & temps** to choose different temperatures for each period. Incomplete previous setup still requires the combined form for scheduled heating; minimum-temperature protection remains available.


## Wi-Fi recovery and reset

The device reconnects automatically. If disconnected for 30 seconds, it opens the same open recovery hotspot. Scheduled control remains active when the clock and hardware are healthy. Minimum-temperature protection needs healthy hardware but no clock. Visit 192.168.4.1 and sign in if protection is enabled to correct Wi-Fi details on the Device page. Save the complete Wi-Fi password when changing networks; blank means an open network.

**Device → Factory reset** requires typing `RESET`. It commands heating off, clears the application's saved Wi-Fi credentials, dashboard password, targets, schedule and probe mapping, clears the ESP32 station credentials, and restarts. Join the open LizardLamps-XXXXXX hotspot and visit http://192.168.4.1 to start again.

The setup hotspot is open and initial provisioning is accessible to nearby connected devices. When protection is enabled, the dashboard uses HTTP Digest authentication. A per-boot form token protects writes in either mode. Wi-Fi and dashboard credentials are stored in device NVS; flash encryption is not configured by this project. It is intended for a trusted local network; do not expose its HTTP service directly to the internet.

## Hardware

- ESP32-WROOM-32 development board.
- Two DS18B20 temperature probes sharing GPIO4, with a 4.7kΩ pull-up to 3V3.
- Two-channel AC TRIAC dimmer with zero-cross detection and ESP32-compatible logic.
- Two compatible 230V heat lamps, approximately 150W each.
- USB supply, protective enclosure, mains protection and independent thermal cutout.

| Component | Signal | ESP32 connection |
| --- | --- | --- |
| AC dimmer | VDD | 3V3 |
| AC dimmer | GND | GND |
| AC dimmer | Z-C | GPIO23 |
| AC dimmer | D1 | GPIO25 |
| AC dimmer | D2 | GPIO26 |
| DS18B20 probes | Data | GPIO4 |
| DS18B20 probes | Power | 3V3 |
| DS18B20 probes | Ground | GND |

This describes intended low-voltage signals, not mains wiring. Verify the exact dimmer's voltage, isolation, signal levels, load compatibility and ratings. The sketch assumes **50Hz mains** and a **10,000µs half-cycle**.

## Build and upload

Clone into a folder named `lizard-lamps`. In Arduino IDE, open `lizard-lamps.ino`, install the Espressif ESP32 core and libraries below, then select your ESP32-WROOM-32 board (typically **ESP32 Dev Module**) and serial port. All root `.h`/`.cpp` files are needed; do not copy only the sketch.

| Dependency | Version used for compilation | Upstream |
| --- | --- | --- |
| Espressif ESP32 core | 3.3.12 | [arduino-esp32](https://github.com/espressif/arduino-esp32) |
| OneWire | 2.3.8 | [OneWire](https://github.com/PaulStoffregen/OneWire) |
| DallasTemperature | 4.0.6 | [Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library) |
| MycilaDimmer | 3.0.1 | [MycilaDimmer](https://github.com/mathieucarbou/MycilaDimmer) |
| MycilaPulseAnalyzer | 3.0.10 | [MycilaPulseAnalyzer](https://github.com/mathieucarbou/MycilaPulseAnalyzer) |

WiFi, WebServer, DNSServer, ESPmDNS and Preferences come with the ESP32 core. The task separation assumes the dual-core ESP32-WROOM-32; single-core variants are not supported by this configuration.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli board list
arduino-cli upload --fqbn esp32:esp32:esp32 --port <your-serial-port> .
arduino-cli monitor --port <your-serial-port> --config baudrate=115200
```

Run tests with a C++17 compiler and Python 3 (Node.js enables the embedded AJAX client tests):

```sh
./tests/run.sh
```

**Device → Dashboard access** lets you enable or disable password protection later. Existing saved passwords stay enabled after an upgrade. Leave the password blank to keep an existing password; enabling from an unprotected state requires a new password. Disabling clears the saved dashboard password and allows anyone on the connected network or recovery hotspot to manage the device.

## On-device API

The browser uses these same-origin endpoints with dashboard Digest authentication when password protection is enabled. Responses are JSON and are not cached. Wi-Fi and dashboard passwords are never returned by the read endpoints.

| Method | Endpoint | Purpose |
| --- | --- | --- |
| GET | `/api/status` | Temperatures, active targets, selected lamp, requested power, clock and health |
| GET | `/api/connection` | Wi-Fi progress, network name, LAN IP and hotspot state |
| GET | `/api/config` | Day/night start times, targets, probe identities and protection state |
| POST | `/api/schedule` | Save both periods and all four targets together |
| POST | `/api/wifi` | Save Wi-Fi details and initial dashboard protection choice |
| POST | `/api/control` | Save dimming mode, ranges and minimum safe temperature |
| POST | `/api/admin` | Enable, disable or change dashboard password protection |
| POST | `/api/clock` | Supply browser-time fallback |
| POST | `/api/probes` | Swap basking/ambient probe identities |
| POST | `/api/reset` | Confirm factory reset |

Writes accept URL-encoded form fields, including the per-boot `csrf` token embedded in the page. The schedule fields are `lamp1`, `lamp2`, `dayBasking`, `dayAmbient`, `nightBasking`, and `nightAmbient`; times use HH:MM. Heating control uses `dimmingEnabled=1` (on) or `0` (off), plus `dimmingRange` when on or `restartDrop` when off, in °C. The inactive range may be omitted to preserve its saved value. `minimumTemperature` optionally sets the 5–40°C floor; omitting it preserves the saved value. `/api/config` returns all four settings. `/api/status` adds `lampPower: [day, night]` and `floorMode` (`0` normal, `1` recovery, `2` night hold); `activeLamp=2` means both lamps during recovery. Dashboard access uses `protectAdmin=1` to enable protection and `adminPassword` for a new password; omit `protectAdmin` to disable it. Initial Wi-Fi setup uses the same fields. CSRF checks apply with or without password protection. Reset also requires `confirmation=RESET`. Optional `epoch` supplies browser time when the clock is unset. Normal HTML POST routes remain available as a fallback when JavaScript is disabled.

Missing/disconnected readings are JSON `null`. The client shows a connection-loss notice when polling fails, avoids overlapping polls, backs off on failures and pauses network requests while the tab is hidden. After Wi-Fi setup, navigation to the connection page is intentional; there are no automatic full-page refreshes.

## Structure and documentation

- `lizard-lamps.ino`: sensor conversions, dimmer hardware and local control loop.
- `ControlLogic.h`: hardware-independent schedule, temperature control and validation.
- `ConfigData.h`: per-period configuration, validation and migration of earlier saved settings.
- `DeviceState.h/.cpp`: synchronized configuration/status and persistent storage.
- `DeviceWeb.cpp`: setup, networking, clock, server-rendered pages and JSON API endpoints.
- `WebClient.h`: AJAX polling, live DOM updates, inline settings saves and connection feedback.
- `WebStyle.h`: embedded responsive design.
- [Safety and limitations](docs/SAFETY.md), [validation](docs/VALIDATION.md), [roadmap](docs/ROADMAP.md), [contributing](CONTRIBUTING.md).
- [Original briefing](docs/PROJECT-BRIEF.md) and [original sketch](docs/original/lizard-lamps.ino) are historical references. The current API supports the local dashboard; there is no external backend.

## License

[MIT](LICENSE), copyright 2026 Caedmon Mullin. Dependencies retain their own licenses. Contributions are welcome.
