# Safety and known limitations

This is an implemented prototype controlling 230V heating equipment, not a certified safety controller. Compilation and mocked tests do not establish electrical, thermal or animal safety.

## Independent hardware protection

Use an independent, mains-rated over-temperature cutout: a failed TRIAC can remain conductive despite zero-duty commands. Mains installation, isolation, enclosure, earthing, circuit protection, cable ratings and thermal suitability must be independently verified by someone competent to perform that work.

Verify the exact dimmer's logic voltage, zero-cross signal, isolation, current rating, heat dissipation and phase-angle compatibility with the lamps. The connection table is not a complete schematic or mains design. Do not assume an unspecified module can safely use 3V3.

## Firmware safeguards

- Output pins are driven low during startup; initialized dimmers receive zero-duty commands.
- Both targets, schedule and a valid clock are required before heating.
- Probes use saved addresses, with no automatic replacement of missing probes.
- Disconnected, non-finite, below -20°C, or at/above 45°C readings disable heating.
- A last-reading timeout of 3 seconds disables heating when the loop can evaluate it.
- Invalid configuration and unavailable zero-cross detection request zero output.
- Lamp switching commands both outputs off and waits 20ms before enabling the new selected lamp.
- Configuration writes wait for the control loop to acknowledge output off; unsuccessful writes do not apply the proposed settings.
- Factory reset waits for output-off acknowledgment before erasing data and restarting.
- HTTP/network activity runs in a separate task from the control loop.

The 45°C threshold is fixed in `ControlLogic.h` and must be assessed for the actual enclosure. It is not an independent thermal cutout. Fault shutdown is not latched: heating may resume once readings and other required conditions are healthy.

Both probes can request heating from the same selected lamp. One warm probe does not prevent a colder probe from requesting full power while temperatures are below the limit.

## Remaining limitations

- Actual lamp power and physical output-off state are not measured.
- A stale-reading check cannot shut down outputs if the processor/control task itself stops executing. There is no application-specific watchdog strategy or independent freshness verification of each DS18B20 conversion.
- Startup/reset pin behavior, TRIAC timing, thermal limits and network/flash interference require real hardware verification.
- Saved addresses are initially assigned by discovery order; users must identify and, if necessary, swap the physical probe labels.
- The clock keeps running after network loss, but cannot retain valid wall time across a power interruption. Browser fallback relies on the user's phone/computer clock; NTP is unauthenticated.
- Settings/credentials are stored in NVS without project-configured flash encryption. The web service uses HTTP and is intended for a trusted local network.
- There are no latched alarms, fault history, scheduled heating-off periods or installation-specific calibration settings.
- In a reset storage failure, heating remains disabled until the reset is retried or the controller is restarted; saved data must not be assumed erased.

See the [hardware validation checklist](VALIDATION.md). The original briefing is historical and does not describe completed safety guarantees.
