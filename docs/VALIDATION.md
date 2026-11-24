# Validation and known limits

[Back to the README](../README.md)

Glasshouse is an original digital interpretation of creative-compressor
behaviors. These results do not establish sonic equivalence to Chase Bliss
Clean or compatibility with its presets.

## Hardware listening — October 30, 2026

The development setup used a stereo Hothouse with an original STM32H750 Daisy
Seed in an HX Stomp effects loop, initially before US Deluxe Nrm. The amp block
was bypassed for later isolation tests. The final installed image was v0.3.2:
113,792 bytes, SHA-256
`d2df8832c751a2231b05fc2a522a26a0306c3a03ec546c89b6122195ed3ab1b7`.

DFU download completed successfully. After reboot, the normal application
passed the protocol-identity-2 USB MIDI round-trip check.

| Area | Observed result |
| --- | --- |
| Bypass / Dynamics-off unity | Passed. |
| Compression | Increasing Dynamics reduced level; early unintended wobble was resolved. |
| Wet / Dry | Both independently swept from silence to normal level. |
| Fixed EQ | Clear high-cut / low-cut response. |
| Dynamic EQ | Threshold opening and Attack shaping audible. Deep cuts produced a large level rise on opening. |
| Release | Short and long settings produced visibly different gain-reduction LED decay. |
| Physics | Audible difference, but subtle in this setup. |
| Sensitivity | Clockwise increased reduction; the extreme setting substantially reduced output. |
| Dynamic swell | Holding the left switch before playing produced the expected fade-in. |
| Maximum sag | Held sag ducked the signal; release restored it. Switching pops were resolved and confirmed on v0.3.2. |
| Attack | Knob values and applied 0.5–245 ms timing verified on hardware; dynamic-EQ opening changed audibly. Compression-transient contrast remains unclear from the initial listening test. |
| Routing isolation | Wet and Dry at zero produced silence with the effect enabled. |

## Corrections included in v0.3.2

- **Compression stability (v0.3):** replaced an unstable delayed feedback gain
  loop with input-derived gain control and made the ratio continuous through
  noon. A steady 110 Hz tone's gain-reduction fluctuation fell from about
  2.1 dB to less than 0.003 dB at the tested positions. The unwanted wobble
  also disappeared in the physical listening comparison.
- **Endpoint pickup (v0.3.1):** the physical Attack stop measured 123 on a
  0–127 readback scale. It could not pick up a stored MIDI value of 127 under
  the original narrow tolerance. Endpoint targets now allow takeover in the
  outer 5% of physical travel; interior pickup keeps its original tolerance.
- **Sag switching (v0.3.2):** smoothed the maximum-sag override and nonlinear
  sag gain with 50 ms time constants. A constant-input test's engagement
  step fell from about 0.19923 to 0.000894 at unity Wet in the cited case;
  release also became smooth. Physical listening confirmed both pops were gone.

## Dynamic EQ is not level-neutral

The dynamic mode opens from the chosen EQ cut toward flat when the detector
crosses the Sensitivity threshold. Attack and Release shape that movement.
It is a threshold-triggered opening, not continuous proportional loudness
tracking. There is no automatic EQ level compensation.

With EQ fully left, a constant-program / separate-detector host test measured
roughly 13.4 dB restoration at 1 kHz and 22.8 dB at 3 kHz on opening. At a
one-third EQ position (roughly 10 o'clock), the respective changes were about
0.2 and 1.8 dB. These are sine-tone measurements, not predictions of guitar
loudness. Begin with a moderate cut and moderate compression for musical use.

## Automated coverage

The host suite checks unity, emphasis cancellation, compression/limiting/sag,
sag transitions, stereo bypass and linked balance, external-key ducking, drive,
gate closure, swells, EQ response, bounce automation, randomized mode changes,
preset checksums and high-slot recall, endpoint pickup, and footswitch conflicts.
Address/undefined-behavior sanitizer runs passed during development. Diagnostic
snapshot encoding and controller/page distinctions have separate host tests.

## Still open

- Full stereo, spread, sidechain and advanced-option hardware checks.
- Preset power-cycle persistence and interrupted-write recovery. Storage uses
  a single bank; a failed/interrupted save can lose the saved bank.
- Automation and USB interoperability under broader real-world use.
- Real-time CPU headroom, codec noise/latency and unconfirmed module revisions.
- More listening calibration of compression Attack and Physics intensity.

The internal Seed LED illuminates when measured average callback load exceeds
80%; that monitor was not observed during these tests. Audio runs at 48 kHz in
48-sample blocks. Total latency also includes codec/driver buffering; the DSP
has no lookahead buffer. Host tests alone cannot validate that runtime behavior.
