// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dsp.h"

namespace glasshouse {
struct Input {
    float knob[6] = {};
    uint8_t toggle[3] = {};
    bool left = false, right = false;
};

class Controller {
public:
    void Init(float rate, const Bank &bank, const Input &input) {
        bank_ = bank;
        input_ = input;
        for (int i = 0; i < 6; ++i) settings_.knob[0][i] = input.knob[i];
        for (int i = 0; i < 3; ++i) settings_.toggle[0][i] = input.toggle[i];
        live_ = settings_;
        ResetPickup();
        engine_.Init(rate, settings_);
        engine_.SetSettings(settings_);
    }
    void Tick(const Input &in, float dt) {
        now_ += dt;
        const bool chord = in.left && in.right;
        if (chord) {
            if (!chord_active_) chord_seconds_ = 0;
            chord_active_ = true;
            chord_seconds_ += dt;
            if (chord_seconds_ >= 3.0f) dfu_requested_ = true;
            engine_.SetAux(false);
            engine_.SetSag(false);
        } else if (chord_active_) {
            // Consume both releases, so a settings/DFU chord cannot also bypass or save.
            if (!in.left && !in.right) {
                if (!dfu_requested_) {
                    page_ = (page_ + 1) % 5;
                    ResetPickup();
                }
                chord_active_ = false;
                left_was_ = right_was_ = false;
                right_seconds_ = 0;
            }
        } else {
            HandleFeet(in, dt);
        }
        if (!chord_active_ && !chord) {
            if (page_ < 4) {
                for (int i = 0; i < 6; ++i) {
                    const float target = settings_.knob[page_][i];
                    const float a = input_.knob[i] - target;
                    const float b = in.knob[i] - target;
                    // ADC end travel can stop short of 0/1. Let those physical
                    // stops pick up endpoint MIDI/preset values without widening
                    // the normal interior pickup window.
                    const bool endpoint = (target <= 0.025f && in.knob[i] <= 0.05f)
                        || (target >= 0.975f && in.knob[i] >= 0.95f);
                    if (std::fabs(b) < 0.025f || a * b < 0 || endpoint) picked_[i] = true;
                    if (picked_[i]) settings_.knob[page_][i] = Clamp(in.knob[i]);
                }
                for (int i = 0; i < 3; ++i)
                    if (in.toggle[i] != input_.toggle[i]) settings_.toggle[page_][i] = in.toggle[i];
            } else {
                if (std::fabs(in.knob[0] - input_.knob[0]) > 0.001f)
                    selected_ = static_cast<unsigned>(Clamp(in.knob[0]) * 7.999f) * 16 + selected_ % 16;
                if (std::fabs(in.knob[1] - input_.knob[1]) > 0.001f)
                    selected_ = selected_ / 16 * 16 + static_cast<unsigned>(Clamp(in.knob[1]) * 15.999f);
                if (selected_ >= kPresets) selected_ = kPresets - 1;
            }
        }
        input_ = in;
        engine_.SetSettings(settings_);
    }
    void MidiCc(unsigned cc, unsigned value) {
        value = value > 127 ? 127 : value;
        if (cc >= 14 && cc <= 37) {
            const unsigned p = (cc - 14) / 6, k = (cc - 14) % 6;
            settings_.knob[p][k] = value / 127.0f;
            if (page_ == p) picked_[k] = false;
        } else if (cc >= 40 && cc <= 51) {
            const unsigned p = (cc - 40) / 3, t = (cc - 40) % 3;
            settings_.toggle[p][t] = value < 43 ? 0 : (value < 85 ? 1 : 2);
        } else if (cc == 102) {
            engine_.SetBypass(value < 64);
        } else if (cc == 103) {
            midi_aux_ = value >= 64;
            engine_.SetAux(midi_aux_ || physical_aux_);
        } else if (cc == 104) {
            midi_sag_ = value >= 64;
            engine_.SetSag(midi_sag_ || physical_sag_);
        } else if (cc == 119 && value >= 1 && value <= kPresets) {
            Save(value - 1);
        }
        engine_.SetSettings(settings_);
    }
    void Program(unsigned program) {
        if (program == 0) {
            if (recalled_) settings_ = live_;
            recalled_ = false;
        } else if (program <= kPresets && bank_.Has(program - 1)) {
            if (!recalled_) live_ = settings_;
            selected_ = program - 1;
            settings_ = bank_.preset[selected_];
            recalled_ = true;
        } else return;
        ResetPickup();
        engine_.SetSettings(settings_);
        engine_.SetAux(false);
        aux_latched_ = false;
        sag_latched_ = false;
        engine_.SetSag(false);
    }
    bool TakeSaveRequest() { bool value = save_requested_; save_requested_ = false; return value; }
    bool DfuRequested() const { return dfu_requested_; }
    const Bank &Presets() const { return bank_; }
    const Settings &Parameters() const { return settings_; }
    Engine &Audio() { return engine_; }
    unsigned Page() const { return page_; }
    float LeftLed() const {
        if (page_ != 0) {
            if (page_ == 4) {
                const float t = std::fmod(now_, 3.0f);
                return t < (selected_ / 16 + 1) * 0.25f && std::fmod(t, 0.25f) < 0.1f ? 1.0f : 0.0f;
            }
            const float t = std::fmod(now_, 2.0f);
            return t < page_ * 0.3f && std::fmod(t, 0.3f) < 0.12f ? 1.0f : 0.0f;
        }
        return physical_aux_ || midi_aux_ ? engine_.Swell() : Clamp(engine_.Reduction() / 24.0f);
    }
    float RightLed() const {
        if (now_ < saved_until_) return std::fmod(now_ * 8.0f, 1.0f) < 0.5f ? 1.0f : 0.0f;
        if (page_ == 4) {
            // 1..16 flashes, then a pause. Empty slots are dimmer.
            const float t = std::fmod(now_, 5.5f);
            return t < (selected_ % 16 + 1) * 0.25f && std::fmod(t, 0.25f) < 0.1f
                ? (bank_.Has(selected_) ? 1.0f : 0.15f) : 0;
        }
        return engine_.Bypassed() ? 0.0f : 1.0f;
    }
private:
    void ResetPickup() { for (auto &v : picked_) v = false; }
    void Save(unsigned slot) {
        if (slot >= kPresets || save_requested_) return;
        bank_.preset[slot] = settings_;
        bank_.Mark(slot);
        bank_.Seal();
        selected_ = slot;
        save_requested_ = true;
        saved_until_ = now_ + 1.0f;
    }
    void HandleFeet(const Input &in, float dt) {
        const bool left_press = in.left && !left_was_;
        const bool right_release = !in.right && right_was_;
        const bool latch = settings_.knob[2][5] > 0.5f;
        if (page_ == 4) {
            physical_aux_ = physical_sag_ = false;
            if (left_press) Program(selected_ + 1);
            if (in.right) right_seconds_ += dt;
            if (right_seconds_ >= 1.0f && !right_long_) { Save(selected_); right_long_ = true; }
        } else {
            const bool manual = settings_.toggle[2][1] == 1;
            if (left_press && latch && !manual) aux_latched_ = !aux_latched_;
            physical_aux_ = manual || !latch ? in.left : aux_latched_;
            if (in.right) right_seconds_ += dt;
            if (right_seconds_ >= 0.4f && in.right && !right_long_) {
                right_long_ = true;
                if (latch) sag_latched_ = !sag_latched_;
            }
            physical_sag_ = latch ? sag_latched_ : in.right && right_seconds_ >= 0.4f;
            if (right_release && !right_long_) engine_.SetBypass(!engine_.Bypassed());
        }
        if (!in.right) { right_seconds_ = 0; right_long_ = false; }
        engine_.SetAux(physical_aux_ || midi_aux_);
        engine_.SetSag(physical_sag_ || midi_sag_);
        left_was_ = in.left;
        right_was_ = in.right;
    }
    Engine engine_;
    Settings settings_, live_;
    Bank bank_;
    Input input_;
    bool picked_[6] = {};
    bool left_was_ = false, right_was_ = false, right_long_ = false;
    bool chord_active_ = false, dfu_requested_ = false, save_requested_ = false;
    bool aux_latched_ = false, sag_latched_ = false, recalled_ = false;
    bool physical_aux_ = false, physical_sag_ = false, midi_aux_ = false, midi_sag_ = false;
    unsigned page_ = 0, selected_ = 0;
    float now_ = 0, chord_seconds_ = 0, right_seconds_ = 0, saved_until_ = 0;
};
} // namespace glasshouse
