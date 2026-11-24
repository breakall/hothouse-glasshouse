# Glasshouse controls

[Back to the README](../README.md) · [USB MIDI](MIDI.md)

## Performance page


Knob numbers follow the Hothouse hardware numbering, 1–3 across the top and 4–6 across the bottom.

| Control | Function |
|---|---|
| Knob 1 | Sensitivity: threshold from −6 to −54 dBFS |
| Knob 2 | Dynamics: increasing compression, limiting, then sag |
| Knob 3 | Wet level: silence to +12 dB; noon is unity |
| Knob 4 | Attack: nominal 0.5–300 ms; also sets motion speed |
| Knob 5 | EQ: left darkens, center flat, right removes bass |
| Knob 6 | Dry level: silence to +12 dB; noon is unity |
| Toggle 1 | Release: up 50 ms, middle custom, down 1.5 s |
| Toggle 2 | EQ: up playing-responsive, middle fixed, down modulated |
| Toggle 3 | Physics: up gentle spring, middle stable, down lively spring |
| Left footswitch | Swell; behavior set on Options page |
| Right footswitch tap | Enable/bypass on release |
| Right footswitch hold ≥0.4 s | Maximum sag; release restores normal dynamics |

Controls are smoothed. Attack times are nominal coefficients, with additional detector response; they have not been matched to Clean's measured timing. There is no automatic makeup gain. Increase Wet to compensate for compression. Strong Wet + Dry settings can intentionally reach the final limiter.

## Settings pages

Briefly press both footswitches together and release both to advance:

**Performance → Envelope → Options → Automation → Presets → Performance.**

The left LED flashes once for Envelope, twice for Options, three times for Automation. Performance uses the left LED for gain reduction or swell level. In Presets, both LEDs display the bank and slot.

Knobs use pickup when changing pages, recalling presets, or receiving MIDI: turn a knob through the stored position before it takes control. Toggle switches update only when physically moved, so entering a page does not replace its stored settings. Pages retain their values while powered; save a preset to retain a configuration across power cycles. Startup always uses the physical performance controls and default advanced options; recall a preset to restore saved advanced options.

On Envelope, Options, and Automation pages, the footswitches still provide swell and bypass/sag. The Presets page replaces those actions with recall/save.

**Hold both footswitches for 3 seconds to enter USB DFU flashing mode.** The LEDs alternate before reset. This replaces the upstream firmware's two-second gesture. A short settings chord never also toggles bypass or saves a preset.

### Envelope page

| Control | Function |
|---|---|
| Knob 1 | Gate threshold: −80 to −30 dBFS |
| Knob 2 | Gate closing time: 20 ms–1.5 s |
| Knob 3 | Swell in: 10 ms–4 s |
| Knob 4 | Swell out: 10 ms–4 s |
| Knob 5 | Custom release: 50 ms–1.5 s |
| Knob 6 | Detector bass rejection: fully left off, then 20–1200 Hz |
| Toggle 1 | Envelope: up fixed, middle fixed attack/adaptive release, down adaptive both |
| Toggle 2 | Spread routing: up EQ, middle EQ + volume, down volume |
| Toggle 3 | Emphasis: up off, middle approximately +3.5 dB, down +6 dB |

Emphasis boosts highs before compression and applies a complementary filter afterward. The pair cancels at unity dynamics; compression can change its tonal result. Detector bass rejection affects control signals, not the direct audio frequency response.

### Options page

| Control | Function |
|---|---|
| Knob 1 | Stereo spread depth |
| Knob 2 | Automation rate: 0.03–8 Hz |
| Knob 3 | Gate: left half off, right half on |
| Knob 4 | Compression motion: left half off, right half on |
| Knob 5 | Dusty-style drive: left half off, right half on |
| Knob 6 | Footswitch latch: left half momentary, right half latching |
| Toggle 1 | Routing: up mono, middle stereo, down left audio + right sidechain |
| Toggle 2 | Swell: up dynamic while Aux active, middle manual trigger, down always dynamic |
| Toggle 3 | Stereo: up linked, middle independent, down spread |

Dynamic swell opens above the sensitivity threshold and closes below it. In manual mode, pressing Aux starts a complete dip and holding it maintains silence; after release and completion of the dip, the signal swells back in. A brief tap still completes the dip. Swell times affect the whole dry/wet mix.

Latch changes dynamic Aux to tap-on/tap-off and sag to hold-on/hold-off. Manual swell remains momentary. Use stereo or spread mode for stereo effects. With Spread routed only to EQ, compression remains linked. Current spread uses phase-offset filtering/panning, separate envelope responses and differing swell times; it does not recreate every mode-specific stereo gesture of Clean.

In sidechain routing, the right input is a detector signal only and is never sent directly to the outputs. Both outputs carry processed left audio. Use a TRS-to-dual-TS insert cable to separate the Hothouse's left and right inputs. The key must be within the Hothouse's instrument-level input range.

### Automation page

Each of the six knobs sets the automation depth for its corresponding performance knob. Zero depth leaves that parameter unchanged.

| Toggle | Up | Middle | Down |
|---|---|---|---|
| 1: mode | Off | One-shot on effect enable | Repeating bounce |
| 2: range | Rise from lower value to knob setting | Fall from knob setting to lower value | Move either side of knob setting, clamped to range |
| 3: shape | Linear | Smooth cosine | Stepped |

Rate is set with Options knob 2. A one-shot restarts when bypass changes to effect-on. Ramping does not repurpose the Sensitivity knob in this implementation. Timing is internal; MIDI clock synchronization is not implemented.

### Presets page

- Knob 1 selects bank 1–8; knob 2 selects slot 1–16. The final bank has 15 slots, for 127 total presets.
- Left LED: bank count; right LED: slot count. Groups repeat with a pause. A dim right LED indicates an empty slot.
- Tap left to recall the selected slot. Empty slots leave the sound unchanged.
- Hold right for one second to save all four settings pages to the selected slot. The right LED blinks to acknowledge the request. Saving fades the output to silence, pauses audio during the flash write, then fades back in. A brief audio interruption is expected.
- A save overwrites the selected slot. Saving is explicit; knob movement does not write flash.

Settings occupy a 64 KiB reservation in the Seed's 8 MiB QSPI flash, starting at offset `0x007a0000`. Preset banks carry a version and checksum; an invalid bank is ignored. Storage uses a single bank, so interrupted writes can lose the saved bank. Power-cycle persistence still needs a hardware test.
