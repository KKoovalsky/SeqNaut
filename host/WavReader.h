#pragma once
#include "AudioNode.h"
#include "dr_wav.h"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

// ── WavReader ─────────────────────────────────────────────────────────────────
//
// AudioNode source: streams a WAV file into the graph block by block.
// Multi-channel files are downmixed to mono on read.
// Produces silence once the file is exhausted.
//
// sampleRate() exposes the file's sample rate so the caller can initialise
// Patch with the correct value before wiring the graph.
//
// numInputs()  = 0  (source node)
// numOutputs() = 1  (mono)

class WavReader : public AudioNode {
public:
    WavReader(const std::string& path, size_t blockSize)
        : _blockSize(blockSize)
        , _outputBus(1, AudioBuffer(blockSize, 0.f))
        , _interleaved(blockSize * 1, 0.f)  // resized after open when channels are known
    {
        if (!drwav_init_file(&_wav, path.c_str(), nullptr))
            throw std::runtime_error("WavReader: cannot open '" + path + "'");

        _open = true;
        _channels = _wav.channels;
        _interleaved.resize(blockSize * _channels);
        _outputView[0] = AudioBufferView(_outputBus[0]);
    }

    ~WavReader() { close(); }

    WavReader(const WavReader&)            = delete;
    WavReader& operator=(const WavReader&) = delete;

    float  sampleRate() const { return static_cast<float>(_wav.sampleRate); }
    bool   done()       const { return !_open; }

    void close() {
        if (_open) {
            drwav_uninit(&_wav);
            _open = false;
        }
    }

    // ── AudioNode ─────────────────────────────────────────────────────────────

    size_t numInputs()  const override { return 0; }
    size_t numOutputs() const override { return 1; }

    AudioBusView process(AudioBusView) override {
        const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(
            &_wav, _blockSize, _interleaved.data());

        if (framesRead == 0) {
            close();
            std::fill(_outputBus[0].begin(), _outputBus[0].end(), 0.f);
            return AudioBusView(_outputView);
        }

        if (_channels == 1) {
            std::copy(_interleaved.begin(),
                      _interleaved.begin() + static_cast<ptrdiff_t>(framesRead),
                      _outputBus[0].begin());
        } else {
            const float scale = 1.f / static_cast<float>(_channels);
            for (drwav_uint64 i = 0; i < framesRead; ++i) {
                float sum = 0.f;
                for (unsigned ch = 0; ch < _channels; ++ch)
                    sum += _interleaved[i * _channels + ch];
                _outputBus[0][i] = sum * scale;
            }
        }

        if (framesRead < _blockSize)
            std::fill(_outputBus[0].begin() + static_cast<ptrdiff_t>(framesRead),
                      _outputBus[0].end(), 0.f);

        return AudioBusView(_outputView);
    }

private:
    drwav                          _wav;
    bool                           _open     = false;
    unsigned                       _channels = 1;
    size_t                         _blockSize;
    AudioBus                       _outputBus;
    std::array<AudioBufferView, 1> _outputView {};
    std::vector<float>             _interleaved;
};
