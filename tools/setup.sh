#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=vendor/HothouseExamples
revision=ff2b5ac0874fd03557eece62dbb3c784562cb7ee
mkdir -p vendor
if [[ ! -d "$upstream/.git" ]]; then
    git clone --no-checkout https://github.com/clevelandmusicco/HothouseExamples.git "$upstream"
    git -C "$upstream" checkout --detach "$revision"
fi
if [[ "$(git -C "$upstream" rev-parse HEAD)" != "$revision" ]]; then
    echo "Expected HothouseExamples revision $revision. Existing vendor checkout was left unchanged." >&2
    exit 1
fi
git -C "$upstream" submodule update --init --depth 1 libDaisy
git -C "$upstream/libDaisy" submodule update --init --depth 1 \
    Drivers/CMSIS_5 Drivers/CMSIS-Device/ST/STM32H7xx \
    Drivers/STM32H7xx_HAL_Driver Middlewares/ST/STM32_USB_Device_Library
make dependencies "$@"
make "$@"
