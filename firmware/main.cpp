// SPDX-License-Identifier: GPL-3.0-or-later
#include <atomic>
#include "hothouse.h"
#include "hid/midi.h"
#include "util/PersistentStorage.h"
#include "util/CpuLoadMeter.h"
#include "controller.h"
#ifdef GLASSHOUSE_DIAGNOSTICS
#include "diagnostics.h"
#endif

using clevelandmusicco::Hothouse;
using namespace glasshouse;
Hothouse hw;
Controller controller;
daisy::MidiUsbHandler midi;
daisy::Led led_left, led_right;
daisy::CpuLoadMeter cpu;
static_assert(ATOMIC_INT_LOCK_FREE == 2, "The audio queue requires lock-free integer atomics");

// Single producer (foreground), single consumer (audio interrupt).
struct Command { uint8_t type, number, value; };
Command queue[32];
std::atomic<unsigned> write_index{0}, read_index{0};
enum class Maintenance : unsigned { None, Save, Dfu };
std::atomic<Maintenance> maintenance{Maintenance::None};
std::atomic<bool> maintenance_ready{false};
float master = 0;
#ifdef GLASSHOUSE_DIAGNOSTICS
enum class DiagnosticState : unsigned { Idle, Requested, Ready };
std::atomic<DiagnosticState> diagnostic_state{DiagnosticState::Idle};
DiagnosticSnapshot diagnostic_snapshot;
float diagnostic_peak = 0;
#endif

void Enqueue(Command command) {
    const unsigned w = write_index.load(std::memory_order_relaxed);
    const unsigned next = (w + 1) % 32;
    if (next == read_index.load(std::memory_order_acquire)) return;
    queue[w] = command;
    write_index.store(next, std::memory_order_release);
}

Input ReadInput() {
    Input in;
    for (int i = 0; i < 6; ++i) in.knob[i] = hw.GetKnobValue(static_cast<Hothouse::Knob>(i));
    for (int i = 0; i < 3; ++i)
        in.toggle[i] = static_cast<uint8_t>(hw.GetToggleswitchPosition(static_cast<Hothouse::Toggleswitch>(i)));
    in.left = hw.switches[Hothouse::FOOTSWITCH_1].Pressed();
    in.right = hw.switches[Hothouse::FOOTSWITCH_2].Pressed();
    return in;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in, daisy::AudioHandle::OutputBuffer out, size_t size) {
    cpu.OnBlockStart();
    if (maintenance.load(std::memory_order_acquire) == Maintenance::None) {
        hw.ProcessAllControls();
        controller.Tick(ReadInput(), size / hw.AudioSampleRate());
        // Bound work even under a flood of incoming MIDI.
        for (int count = 0; count < 8; ++count) {
            const unsigned r = read_index.load(std::memory_order_relaxed);
            if (r == write_index.load(std::memory_order_acquire)) break;
            const Command command = queue[r];
            read_index.store((r + 1) % 32, std::memory_order_release);
            if (command.type == 0) controller.MidiCc(command.number, command.value);
            else controller.Program(command.number);
        }
        if (controller.DfuRequested()) maintenance.store(Maintenance::Dfu, std::memory_order_release);
        else if (controller.TakeSaveRequest()) maintenance.store(Maintenance::Save, std::memory_order_release);
    }
    const bool muted = maintenance.load(std::memory_order_acquire) != Maintenance::None;
    const float step = 1.0f / (0.02f * hw.AudioSampleRate());
    for (size_t i = 0; i < size; ++i) {
#ifdef GLASSHOUSE_DIAGNOSTICS
        diagnostic_peak = std::fmax(diagnostic_peak, std::fabs(in[0][i]));
#endif
        float l, r;
        controller.Audio().Process(in[0][i], in[1][i], l, r);
        master = Clamp(master + (muted ? -step : step));
        out[0][i] = master * l;
        out[1][i] = master * r;
    }
#ifdef GLASSHOUSE_DIAGNOSTICS
    if (diagnostic_state.load(std::memory_order_acquire) == DiagnosticState::Requested) {
        diagnostic_snapshot.Capture(ReadInput(), controller, diagnostic_peak);
        diagnostic_peak = 0;
        diagnostic_state.store(DiagnosticState::Ready, std::memory_order_release);
    }
#endif
    led_left.Set(controller.LeftLed());
    led_right.Set(controller.RightLed());
    led_left.Update();
    led_right.Update();
    if (muted && master == 0) maintenance_ready.store(true, std::memory_order_release);
    cpu.OnBlockEnd();
    hw.seed.SetLed(cpu.GetAvgCpuLoad() > 0.8f);
}

int main() {
    hw.Init(true); // Original Seed at 480 MHz.
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48);
    led_left.Init(hw.seed.GetPin(Hothouse::LED_1), false, hw.AudioCallbackRate());
    led_right.Init(hw.seed.GetPin(Hothouse::LED_2), false, hw.AudioCallbackRate());
    // Construct large preset storage only after Seed has initialized SDRAM.
    // Keep DTCM available for audio state and the flash writer's stack frame.
    DSY_SDRAM_BSS static daisy::PersistentStorage<Bank> storage(hw.seed.qspi);
    DSY_SDRAM_BSS static Bank defaults;
    defaults.Seal();
    // Separate from NAM capture slots (0x7b0000..), reverb config and presets.
    storage.Init(defaults, 0x007a0000);
    if (!storage.GetSettings().Valid()) storage.GetSettings() = defaults;
    hw.StartAdc();
    for (int i = 0; i < 300; ++i) { hw.ProcessAllControls(); hw.DelayMs(1); }
    controller.Init(hw.AudioSampleRate(), storage.GetSettings(), ReadInput());
    cpu.Init(hw.AudioSampleRate(), hw.AudioBlockSize());
    daisy::MidiUsbHandler::Config midi_config;
    midi_config.transport_config.periph = daisy::MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_config);
    midi.StartReceive();
    hw.StartAudio(AudioCallback);
    while (true) {
        midi.Listen();
        while (midi.HasEvents()) {
            const auto event = midi.PopEvent();
            if (event.channel != 0) continue; // MIDI channel 1
            if (event.type == daisy::ControlChange) {
                auto cc = event;
                const auto data = cc.AsControlChange();
                if (data.control_number == 118 && data.value == 127) {
#ifdef GLASSHOUSE_DIAGNOSTICS
                    uint8_t reply[] = {0xb0, 118, 3}; // Temporary diagnostic build.
#else
                    uint8_t reply[] = {0xb0, 118, 2}; // Glasshouse firmware protocol version.
#endif
                    midi.SendMessage(reply, sizeof(reply));
#ifdef GLASSHOUSE_DIAGNOSTICS
                } else if (data.control_number == 117 && data.value == 127) {
                    auto idle = DiagnosticState::Idle;
                    diagnostic_state.compare_exchange_strong(idle, DiagnosticState::Requested,
                                                              std::memory_order_release);
#endif
                } else {
                    Enqueue({0, data.control_number, data.value});
                }
            } else if (event.type == daisy::ProgramChange) {
                auto pc = event;
                Enqueue({1, pc.AsProgramChange().program, 0});
            }
        }
#ifdef GLASSHOUSE_DIAGNOSTICS
        if (diagnostic_state.load(std::memory_order_acquire) == DiagnosticState::Ready) {
            midi.SendMessage(diagnostic_snapshot.bytes, sizeof(diagnostic_snapshot.bytes));
            diagnostic_state.store(DiagnosticState::Idle, std::memory_order_release);
        }
#endif
        if (maintenance_ready.load(std::memory_order_acquire)) {
            hw.StopAudio(); // Only foreground code accesses flash; audio has faded to zero.
            if (maintenance.load(std::memory_order_acquire) == Maintenance::Dfu) {
                hw.StopAdc();
                for (int i = 0; i < 6; ++i) {
                    led_left.Set(i % 2); led_right.Set(1 - i % 2);
                    led_left.Update(); led_right.Update();
                    hw.DelayMs(100);
                }
                daisy::System::ResetToBootloader(daisy::System::BootloaderMode::DAISY);
            } else {
                storage.GetSettings() = controller.Presets();
                storage.Save();
                maintenance_ready.store(false, std::memory_order_release);
                maintenance.store(Maintenance::None, std::memory_order_release);
                hw.StartAudio(AudioCallback);
            }
        }
        hw.DelayMs(1);
    }
}
