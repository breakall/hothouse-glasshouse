# USB MIDI

[Back to the README](../README.md)


Connect a USB data cable to a computer or USB MIDI host. This firmware receives on **MIDI channel 1**. There is no TRS/DIN MIDI input on stock Hothouse, and this MIDI mapping is specific to Glasshouse.

| Message | Function |
|---|---|
| CC 14–19 | Performance knobs 1–6 |
| CC 20–25 | Envelope knobs 1–6 |
| CC 26–31 | Options knobs 1–6 |
| CC 32–37 | Automation depth knobs 1–6 |
| CC 40–42 | Performance toggles 1–3 |
| CC 43–45 | Envelope toggles 1–3 |
| CC 46–48 | Options toggles 1–3 |
| CC 49–51 | Automation toggles 1–3 |
| CC 102 | 0–63 bypass, 64–127 effect on |
| CC 103 | 0–63 release Aux, 64–127 press Aux |
| CC 104 | 0–63 release maximum sag, 64–127 engage |
| CC 118, value 127 | Identity query; replies with CC 118, value 2, on channel 1 |
| CC 119 | Values 1–127 save to corresponding preset |
| Program Change 1–127 | Recall corresponding preset |
| Program Change 0 | Restore live settings captured before the most recent preset recall |

Knob CC values map linearly to normalized knob positions; switch CC ranges are 0–42 up, 43–84 middle, 85–127 down. Some MIDI applications display programs 1–128 instead of bytes 0–127; consult their numbering convention. There is a bounded MIDI queue; extreme message floods can drop commands. No MIDI echo or parameter dump is implemented.

## Verify the running firmware

On macOS, with a USB data cable connected:

```sh
swift -module-cache-path /tmp/glasshouse-swift-cache tools/probe_midi.swift
```

A normal Glasshouse build replies with protocol identity **2**. This verifies
application boot and a USB MIDI round trip; it does not identify a particular
firmware release. The v0.3.2 binary is identified by its SHA-256 checksum.

## Developer diagnostic build

`make diagnostic` enables read-only control snapshots and uses identity **3**.
It is a separate development image, not the ready-to-flash release. On macOS:

```sh
swiftc -module-cache-path /tmp/glasshouse-swift-cache tools/inspect_controls.swift -o build/inspect-controls
./build/inspect-controls 10
```

The optional second argument sets Attack (0–127) before reading. Without it,
the tool only reads. It prints physical and active knob values, page, bypass,
envelope mode, applied attack/release times, input peak and gain reduction.
Normal release firmware does not implement snapshot readback.

Snapshot query: **CC 117 = 127**. Replies: CC 60–65 physical knobs; 66–71
Performance knobs; 72 page; 73 bypass; 74 envelope mode; 75 gain reduction
(0–127 maps to 0–72 dB); 76/77 attack and 79/80 release as 14-bit MSB/LSB
values in 0.1 ms units; 78 input peak (0–127 maps to −96–0 dBFS).
CC 117 = 3 completes a snapshot.
