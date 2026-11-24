# Install Glasshouse

[Back to the README](../README.md)

## Before you start

This image is for a stereo Cleveland Music Co. Hothouse fitted with an original
STM32H750 Daisy Seed and an existing **Daisy bootloader**. It is not a Seed3
image. Installing it replaces the current effects application; keep a known-good
firmware file available for recovery. Lower the listening volume first.

You'll need a USB data cable and `dfu-util` on your computer. The firmware runs
from SRAM after loading from external QSPI flash. Its application address is
**`0x90040000`**. Do not write this image to `0x08000000` or through STM32 ROM DFU.

## Install the ready-to-flash ZIP

1. Download and unzip `glasshouse-v0.3.2.zip` from the
   [v0.3.2 release](https://github.com/breakall/hothouse-glasshouse/releases/tag/v0.3.2).
2. In the extracted directory, verify the image:

   ```sh
   shasum -a 256 -c SHA256SUMS
   ```

   On Linux, `sha256sum -c SHA256SUMS` is equivalent.
3. Connect the pedal by USB. Start the supplied watcher:

   ```sh
   bash wait_and_flash_hothouse.sh glasshouse-seed.bin
   ```

4. With Glasshouse already running, hold **both footswitches for at least
   three seconds**, then release when the LEDs alternate. If another firmware
   is installed, use its own Daisy-bootloader entry procedure instead.
5. Wait for **`File downloaded successfully`**. The updater asks the pedal to
   leave DFU and reboot. If it stays in update mode, power-cycle it after the
   download has completed.

The watcher waits up to 120 seconds, checks for a Daisy QSPI DFU target, and
writes only after that target appears. It refuses the STM32 internal-flash
ROM target. Run it with direct USB access; a sandbox can hide the pedal.

## Install from a source checkout

From the repository root:

```sh
shasum -a 256 -c release/SHA256SUMS
bash tools/wait_and_flash_hothouse.sh release/glasshouse-seed.bin
```

Use the same footswitch gesture. Glasshouse starts **bypassed** after reboot.
Set Wet to noon, Dry fully off and all toggles centered, then tap and release
the right footswitch to enable it. Default routing is mono input to both outputs.

## Verify application boot

On macOS, this read-only query checks the USB MIDI round trip:

```sh
swift -module-cache-path /tmp/glasshouse-swift-cache tools/probe_midi.swift
```

The normal firmware replies with protocol identity **2**. The identity is not
a release number. SHA-256 identifies the exact downloaded image.

## If the pedal is not detected

- Check the USB cable carries data and that the pedal has power.
- Start the watcher before entering the bootloader. If it times out, restart it
  and repeat the entry procedure.
- `dfu-util -l` should show a Daisy external-flash target with an address in
  the `0x900...` range. An **Internal Flash** target is the wrong bootloader
  for this image; the supplied watcher will not write to it.
- If the application does not start, re-enter the Daisy bootloader and flash
  the known-good Hothouse image you kept. Follow that image's own instructions.
- Bootloader installation or repair is separate from installing this
  application. Use the [official Hothouse documentation](https://github.com/clevelandmusicco/HothouseExamples/wiki)
  for the relevant hardware revision.
