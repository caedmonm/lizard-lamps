# Contributing

Issues and pull requests are welcome. Read the [README](README.md), [briefing](docs/PROJECT-BRIEF.md), [roadmap](docs/ROADMAP.md) and [safety notes](docs/SAFETY.md) first.

Keep changes focused and lightweight enough for an ESP32. Preserve the existing Mycila hardware integration unless a change is justified and validated. Keep temperature-control calculations separate from hardware. The product hosts its dashboard directly on the ESP32 with embedded AJAX and on-device JSON endpoints. Preserve authentication, CSRF checks and hardware-independent control when changing the API. Do not add external services without discussing the architecture.

For bug reports, include the board and dimmer model, ESP32 core and library versions, configuration, expected/actual behavior, reproduction steps and relevant Serial Monitor output. Remove Wi-Fi passwords, tokens and other private details before posting.

For firmware pull requests, describe the resulting behavior and fault implications. Run `./tests/run.sh`, compile for the supported ESP32 target and state exactly which hardware checks were performed. Do not claim runtime or safety validation based on compilation alone. Add meaningful tests when changing control calculations or validation rules.

By contributing, you agree that your contributions will be distributed under this project's MIT license. Third-party libraries retain their own licenses.
