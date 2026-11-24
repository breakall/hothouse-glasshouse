# Build Glasshouse

[Back to the README](../README.md) · [Flashing](FLASHING.md)

## Host tests

The audio/control suite needs a C++14 host compiler and Make. It does not need
an Arm toolchain, hardware or downloaded dependencies:

```sh
make test
```

For address/undefined-behavior checks on a compiler that supports them:

```sh
mkdir -p build
c++ -std=c++14 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Wall -Wextra -Werror -Ifirmware \
  tests/test.cpp -o build/test-sanitized
./build/test-sanitized
```

## Firmware build

Install Git, Make and a complete Arm GNU embedded toolchain, including
newlib/libstdc++ headers and nano libraries. A compiler binary alone is not
sufficient. The distributed v0.3.2 image was built with Arm GNU Toolchain
13.2.Rel1 for macOS arm64.

```sh
bash tools/setup.sh
make release
```

If the toolchain is outside PATH, pass its **bin directory**:

```sh
bash tools/setup.sh GCC_PATH=/absolute/path/to/arm-toolchain/bin
make release GCC_PATH=/absolute/path/to/arm-toolchain/bin
```

Setup fetches HothouseExamples at
`ff2b5ac0874fd03557eece62dbb3c784562cb7ee`, its pinned libDaisy at
`f7727edb9a1febdd174b5310a7bc65340dae8700`, and the required upstream drivers.
DaisySP is not used. Dependencies live in ignored `vendor/`; their upstream
copyright/license notices remain intact.

The application uses `BOOT_SRAM`, 48 kHz audio, 48-sample blocks and the standard
Hothouse pin mapping. `make release` puts the image at
`release/glasshouse-seed.bin` and regenerates `release/SHA256SUMS`.

A successful build does not establish real-time CPU headroom or validate a new
hardware revision. See [validation notes](VALIDATION.md).

## Developer diagnostics

```sh
make diagnostic GCC_PATH=/absolute/path/to/arm-toolchain/bin
c++ -std=c++14 -O2 -DGLASSHOUSE_DIAGNOSTICS -Wall -Wextra -Werror \
  -Ifirmware tests/diagnostics.cpp -o build/test-diagnostics
./build/test-diagnostics
```

The diagnostic image is `release/glasshouse-diagnostic.bin`. It enables
`GLASSHOUSE_DIAGNOSTICS`, uses protocol identity **3**, and adds the readback
protocol in the [MIDI guide](MIDI.md). It is not the normal release image and
should be installed deliberately as a separate development build.
