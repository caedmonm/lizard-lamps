# Safety and known limitations

This is an early prototype controlling 230V mains heating equipment. The software has not been certified or validated as a safety controller. Compilation alone cannot validate hardware operation.

## Independent hardware protection

Use an independent, mains-rated over-temperature cutout: a failed TRIAC can remain conductive even when the firmware requests zero power. Mains installation, isolation, enclosure, earthing, circuit protection, cable ratings and thermal suitability must be independently verified by someone competent to perform that work.

Verify the exact dimmer's logic voltage, zero-cross signal, isolation, current ratings, heat dissipation and compatibility with the chosen lamps. The supplied connection table is not a schematic or a complete mains design. Do not assume an unspecified module can be powered from 3V3 safely.

## What the supplied sketch checks

- Heating demand starts at zero while sensor health is false.
- After successful dimmer startup, both channels receive a zero-duty command before normal control.
- Either disconnected or non-finite sensor reading makes sensor health false and requests zero output.
- Invalid active lamp selection or a non-positive fade range requests zero output.
- Loss of zero-cross detection requests zero output when the dimmer is initialized.
- Normal output commands give power only to the selected lamp.

These are software commands, not verification that a lamp is physically off. Output behavior during boot, reset, partial initialization, power loss and hardware faults must be checked on the real hardware.

## Gaps to address before dependable deployment

- Sensor readings use discovery indices rather than fixed OneWire addresses; probe identities can change.
- There is no independent stale-reading timeout or conversion-freshness verification.
- Validation only rejects disconnected and non-finite readings. There is no explicit plausibility range, startup-value handling or software maximum-temperature cutoff.
- Target temperatures have no bounds or explicit finite-value validation.
- There is no watchdog strategy, latched fault state, alarm or fault history.
- The sketch does not measure actual lamp power or verify that an output command took effect.
- Startup currently includes a one-second delay; output pin behavior before initialization needs hardware verification.
- The zero-cross health check cannot protect against a shorted TRIAC or an unresponsive processor.
- API, scheduling, persistence and network isolation requirements are not yet implemented.

Both sensors can request heating from the selected lamp. One warm sensor does not prevent a colder sensor from requesting full output. Independent over-temperature protection is therefore essential; setpoints alone do not provide a maximum-temperature limit.

## Validation to report

Record board and dimmer model, core/library versions, lamp type, probe addresses and wiring arrangement. Check startup/reset, each probe disconnect, zero-cross loss, lamp selection and power behavior under controlled conditions with appropriate independent protection. Record what was actually tested and what remains unverified.

The original briefing's [safety requirements](PROJECT-BRIEF.md#6-safety-and-reliability) describe intended behavior, not completed guarantees.
