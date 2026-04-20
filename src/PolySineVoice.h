#pragma once
#include <cmath>
#include <cstdint>

#include "IInstrument.h"
#include "Synthesis/oscillator.h"

// PolySineVoice — 8-voice polyphonic sine oscillator bank.
//
// Implements IInstrument so the sequencer can drive it identically to any
// monophonic voice.  Voice allocation uses a free-slot-first policy; when
// all 8 voices are busy the oldest active note is stolen.
//
// Audio rendering:
//   Call process(float* out, size_t len) from your audio callback to sum all
//   active oscillators into the output buffer.  This is intentionally kept
//   separate from IInstrument so that the class compiles and links on the
//   host without any audio driver.

class PolySineVoice : public IInstrument {
public:
    static constexpr int NUM_VOICES = 8;
    static constexpr float SAMPLE_RATE = 44100.0f;

    PolySineVoice() {
        for (int i = 0; i < NUM_VOICES; ++i) {
            oscillators_[i].Init(SAMPLE_RATE);
            oscillators_[i].SetWaveform(daisysp::Oscillator::WAVE_SIN);
            oscillators_[i].SetAmp(0.0f);
            oscillators_[i].SetFreq(440.0f);
            voices_[i] = {};
        }
    }

    // ── IInstrument ───────────────────────────────────────────────────────────

    void noteOn(uint8_t pitch, uint8_t velocity) override {
        const int slot = findFreeVoice();
        voices_[slot] = {pitch, true, allocTick_++};
        oscillators_[slot].SetFreq(midiToHz(pitch));
        oscillators_[slot].SetAmp(velocity / 127.0f);
    }

    void noteOff(uint8_t pitch) override {
        for (int i = 0; i < NUM_VOICES; ++i) {
            if (voices_[i].active && voices_[i].pitch == pitch) {
                voices_[i].active = false;
                oscillators_[i].SetAmp(0.0f);
                return;
            }
        }
    }

    // ── Audio rendering ───────────────────────────────────────────────────────

    // Sum all active voices into `out`, replacing its contents.
    // Call from a block-processing callback at SAMPLE_RATE.
    void process(float* out, size_t numSamples) {
        for (size_t n = 0; n < numSamples; ++n) {
            float sample = 0.0f;
            for (int i = 0; i < NUM_VOICES; ++i)
                sample += oscillators_[i].Process();
            out[n] = sample;
        }
    }

private:
    struct VoiceSlot {
        uint8_t pitch = 0;
        bool active = false;
        uint32_t born = 0;  // allocTick_ at noteOn — used for oldest-note stealing
    };

    daisysp::Oscillator oscillators_[NUM_VOICES];
    VoiceSlot voices_[NUM_VOICES];
    uint32_t allocTick_ = 0;

    // Returns index of a free slot, or the oldest active voice.
    int findFreeVoice() const {
        for (int i = 0; i < NUM_VOICES; ++i)
            if (!voices_[i].active)
                return i;

        // All slots busy — steal oldest note.
        int oldest = 0;
        for (int i = 1; i < NUM_VOICES; ++i)
            if (voices_[i].born < voices_[oldest].born)
                oldest = i;
        return oldest;
    }

    static float midiToHz(uint8_t note) {
        return 440.0f * std::pow(2.0f, static_cast<float>(static_cast<int8_t>(note - 69)) / 12.0f);
    }
};
