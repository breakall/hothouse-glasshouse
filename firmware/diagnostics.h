// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller.h"

// Temporary diagnostic build only. CC 117=127 requests one read-only snapshot.
// Snapshot bytes pass from audio to foreground through a release/acquire flag.
namespace glasshouse {
struct DiagnosticSnapshot {
    uint8_t bytes[66] = {};
    void Put(unsigned index, unsigned cc, unsigned value) {
        bytes[index * 3] = 0xb0;
        bytes[index * 3 + 1] = cc;
        bytes[index * 3 + 2] = value;
    }
    static unsigned Unit(float value) {
        return static_cast<unsigned>(Clamp(value) * 127.0f + 0.5f);
    }
    void Time(unsigned index, unsigned cc, float seconds) {
        const unsigned value = static_cast<unsigned>(Clamp(seconds * 10000.0f, 0, 16383) + 0.5f);
        Put(index, cc, value >> 7);
        Put(index + 1, cc + 1, value & 127);
    }
    void Capture(const Input &physical, Controller &controller, float input_peak) {
        for (unsigned i = 0; i < 6; ++i) {
            Put(i, 60 + i, Unit(physical.knob[i]));
            Put(6 + i, 66 + i, Unit(controller.Parameters().knob[0][i]));
        }
        Put(12, 72, controller.Page());
        Put(13, 73, controller.Audio().Bypassed() ? 1 : 0);
        Put(14, 74, controller.Parameters().toggle[1][0]);
        Put(15, 75, Unit(controller.Audio().Reduction() / 72.0f));
        Time(16, 76, controller.Audio().DiagnosticAttackSeconds());
        Put(18, 78, Unit((GainDb(input_peak) + 96.0f) / 96.0f));
        Time(19, 79, controller.Audio().DiagnosticReleaseSeconds());
        Put(21, 117, 3); // Completion marker and diagnostic version.
    }
};
} // namespace glasshouse
