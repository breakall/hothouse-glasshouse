// SPDX-License-Identifier: GPL-3.0-or-later
#include "diagnostics.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace glasshouse;
void Require(bool condition) { if (!condition) std::exit(1); }
int main() {
    Bank bank; bank.Seal();
    Input in;
    for (int i = 0; i < 6; ++i) in.knob[i] = 0.5f;
    in.knob[5] = 0;
    in.toggle[0] = in.toggle[1] = in.toggle[2] = 1;
    Controller c; c.Init(48000, bank, in);
    c.MidiCc(102, 127);
    DiagnosticSnapshot snapshot;
    auto value = [&](unsigned cc) {
        for (unsigned i = 0; i < sizeof(snapshot.bytes); i += 3)
            if (snapshot.bytes[i + 1] == cc) return unsigned(snapshot.bytes[i + 2]);
        std::exit(1);
    };
    for (unsigned attack : {0u, 127u}) {
        c.MidiCc(17, attack);
        for (int block = 0; block < 1000; ++block) {
            c.Tick(in, 0.001f);
            for (int i = 0; i < 48; ++i) { float l, r; c.Audio().Process(0, 0, l, r); }
        }
        snapshot.Capture(in, c, 0.2f);
        Require(value(63) == 64); // Physical knob stayed at noon.
        Require(value(69) == attack); // MIDI setting is independent of that knob.
        const float ms = ((value(76) << 7) | value(77)) / 10.0f;
        Require(attack ? ms > 299 && ms < 301 : ms > 0.4f && ms < 0.6f);
        Require(value(72) == 0 && value(73) == 0 && value(74) == 1);
        Require(value(117) == 3);
        Require(value(78) > 100 && value(78) < 112); // Peak near -14 dBFS.
        for (unsigned i = 0; i < sizeof(snapshot.bytes); i += 3)
            Require(snapshot.bytes[i] == 0xb0 && snapshot.bytes[i + 1] < 128 && snapshot.bytes[i + 2] < 128);
        std::printf("Diagnostic snapshot: physical=64, active Attack=%u, applied %.1f ms\n", attack, ms);
    }
    // A knob can move on another settings page without moving Performance Attack.
    in.left = in.right = true; c.Tick(in, 0.001f);
    in.left = in.right = false; c.Tick(in, 0.001f);
    in.knob[3] = 0; c.Tick(in, 0.001f);
    snapshot.Capture(in, c, 0);
    Require(value(72) == 1 && value(63) == 0 && value(69) == 127);
    std::puts("PASS: diagnostic values distinguish physical knobs, MIDI settings, pages and applied timing");
}
