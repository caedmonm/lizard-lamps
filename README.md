# Lizard Lamps — ESP32 Dual Heat Lamp Controller

An open source ESP32 controller for two AC heat lamps, originally built for a reptile enclosure. Two DS18B20 probes independently request heat, and a two-channel TRIAC dimmer applies proportional power to one selected lamp.

**Status: early hardware prototype.** The Arduino sketch is preserved as supplied. Wi-Fi, REST API, persistent configuration, schedules and a web interface are planned, not implemented.

> This project controls 230V mains heating equipment. Use a suitable protective enclosure, mains protection and an independent, mains-rated over-temperature cutout. A TRIAC can fail short-circuit, leaving a lamp on despite software commands. See [safety and known limitations](docs/SAFETY.md) before using the hardware.

## How it works

Both sensors influence the selected lamp; sensors are not paired exclusively with individual lamps. Only one lamp receives power at a time.

```text
largest deficit = max(target 1 - temperature 1, target 2 - temperature 2)
requested power = clamp(largest deficit / fade range, 0, 1) × 100%
```

The sketch rounds requested power to a whole percentage and enables the Mycila power lookup table. Output is a requested control percentage, not measured electrical power.

| Greatest temperature deficit | Requested power |
| --- | --- |
| 0°C or less | 0% |
| 0.5°C | 17% |
| 1°C | 33% |
| 1.5°C | 50% |
| 2°C | 67% |
| 2.5°C | 83% |
| 3°C or greater | 100% |

The supplied firmware defaults to **19°C for both targets**, **lamp 1** (`activeLamp = 0`) and a **3°C fade range**. The original briefing uses 15°C as an illustrative initial configuration. These are development values, not recommended husbandry temperatures.

Temperatures are requested approximately once per second using asynchronous 12-bit conversions with a 750ms wait. The sketch commands outputs off when a sensor reading is non-finite or disconnected, lamp selection is invalid, dimmer initialization fails, or zero-cross detection goes offline. Additional fault handling remains necessary; see [known limitations](docs/SAFETY.md).

## Hardware

- ESP32-WROOM-32 development board.
- Two DS18B20 temperature probes sharing a OneWire bus, with a 4.7kΩ pull-up to 3V3.
- Two-channel AC TRIAC dimmer with zero-cross detection and ESP32-compatible logic.
- Two 230V heat lamps, approximately 150W each; verify compatibility with phase-angle dimming.
- USB power supply, protective enclosure, mains protection and independent thermal cutout.

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

This table describes the intended low-voltage signals, not mains wiring instructions. The exact dimmer model, isolation, signal levels and load ratings must be verified. The sketch assumes **50Hz mains** and a **10,000µs half-cycle**.

## Build and upload

1. Clone this repository into a folder named `lizard-lamps` so Arduino's sketch folder matches `lizard-lamps.ino`.
2. Install the Espressif **ESP32 Arduino Core 3.x** and select the board matching your ESP32-WROOM-32 development board (typically **ESP32 Dev Module**).
3. Install the libraries listed below using Arduino IDE's Library Manager.
4. Open `lizard-lamps.ino`, review its configuration, select your board and serial port, and compile/upload.
5. Open Serial Monitor at **115200 baud** to inspect temperatures, targets, selected lamp and requested power.

| Dependency | Version used for the initial build check | Upstream |
| --- | --- | --- |
| Espressif ESP32 core | 3.3.12 | [arduino-esp32](https://github.com/espressif/arduino-esp32) |
| OneWire | 2.3.8 | [OneWire](https://github.com/PaulStoffregen/OneWire) |
| DallasTemperature | 4.0.6 | [Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library) |
| MycilaDimmer | 3.0.1 | [MycilaDimmer](https://github.com/mathieucarbou/MycilaDimmer) |
| MycilaPulseAnalyzer | 3.0.10 | [MycilaPulseAnalyzer](https://github.com/mathieucarbou/MycilaPulseAnalyzer) |

For Arduino CLI with these dependencies installed:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli board list
arduino-cli upload --fqbn esp32:esp32:esp32 --port <your-serial-port> .
arduino-cli monitor --port <your-serial-port> --config baudrate=115200
```

A successful compile does not establish electrical, thermal or animal safety. Hardware validation is separate.

## Configuration

Current configuration lives near the top of `lizard-lamps.ino`:

```cpp
float targetTemperatures[2] = {19.0, 19.0};
int activeLamp = 0; // 0 = lamp 1, 1 = lamp 2
const float FADE_TEMP_RANGE = 3.0;
```

Change these values and rebuild/upload. There is currently no network interface or persistent settings store. Sensor identities currently use OneWire discovery indices, which may change; fixed hardware addresses are a priority.

## Project documentation

- [Complete original briefing](docs/PROJECT-BRIEF.md): hardware, algorithm, proposed endpoints, example status JSON, schedules, architecture and development requirements.
- [Roadmap](docs/ROADMAP.md): staged implementation plan and unresolved design decisions.
- [Safety and known limitations](docs/SAFETY.md): current behavior, gaps and hardware validation needs.
- [Contributing](CONTRIBUTING.md): how to propose changes and report results.

The long-term goal is a standalone Wi-Fi controller with local weekly schedules, a REST API and a responsive web interface. Heating control should continue autonomously when networking is unavailable.

## License

[MIT](LICENSE), copyright 2026 Caedmon Mullin. Dependencies retain their own licenses. Contributions are welcome.
