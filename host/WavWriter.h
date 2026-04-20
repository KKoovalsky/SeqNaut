#pragma once
#include <stdexcept>
#include <string>

#include "AudioNode.h"
#include "dr_wav.h"

// ── WavWriter ─────────────────────────────────────────────────────────────────
//
// AudioNode sink: receives one mono input channel and writes it to a WAV file.
// Uses IEEE 754 32-bit float format — no int16 conversion, no precision loss.
//
// The file is finalised either by calling close() explicitly or automatically
// on destruction.  After close() the node is no longer usable.
//
// numInputs()  = 1
// numOutputs() = 0  — process() returns an empty AudioBusView

class WavWriter : public AudioNode {
public:
    WavWriter(const std::string& path, float sampleRate) {
        drwav_data_format fmt;
        fmt.container = drwav_container_riff;
        fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        fmt.channels = 1;
        fmt.sampleRate = static_cast<drwav_uint32>(sampleRate);
        fmt.bitsPerSample = 32;

        if (!drwav_init_file_write(&_wav, path.c_str(), &fmt, nullptr))
            throw std::runtime_error("WavWriter: cannot open '" + path + "'");

        _open = true;
    }

    ~WavWriter() {
        close();
    }

    // Non-copyable, non-movable — drwav holds internal file state
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    void close() {
        if (_open) {
            drwav_uninit(&_wav);
            _open = false;
        }
    }

    // ── AudioNode ─────────────────────────────────────────────────────────────

    size_t numInputs() const override {
        return 1;
    }
    size_t numOutputs() const override {
        return 0;
    }

    AudioBusView process(AudioBusView inputs) override {
        const AudioBufferView in = inputs[0];
        drwav_write_pcm_frames(&_wav, in.size(), in.data());
        return {};
    }

private:
    drwav _wav;
    bool _open = false;
};
