#pragma once
#include <Audio.h>
#include "IInstrument.h"

// Adapter: AudioSynthWaveform × IInstrument.
//
// Inherits from both so it can live in the Audio graph and the sequencer
// simultaneously — no wrapper, no internal AudioConnection.
//
// Audio world usage:
//   SawtoothVoice   voice;
//   AudioMixer4     mixer;
//   AudioConnection c(voice, 0, mixer, 0);
//
// Sequencer world usage:
//   roll.setInstrument(voice);

struct SawtoothVoice : public AudioSynthWaveform, public IInstrument {

    SawtoothVoice() {
        begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
        amplitude(0.0f);
    }

    void noteOn(uint8_t pitch, uint8_t velocity) override {
        frequency(midiToHz(pitch));
        amplitude(velocity / 127.0f);
    }

    void noteOff(uint8_t /*pitch*/) override {
        amplitude(0.0f);
    }

private:
    static float midiToHz(uint8_t note) {
        return 440.0f * powf(2.0f,
            static_cast<float>(static_cast<int8_t>(note - 69)) / 12.0f);
    }
};
