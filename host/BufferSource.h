#pragma once
#include "AudioNode.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

// ── BufferSource ──────────────────────────────────────────────────────────────
//
// AudioNode source that feeds from a pre-loaded float buffer, block by block.
// Produces silence once the buffer is exhausted.
//
// numInputs()  = 0  (source node — drives the graph)
// numOutputs() = 1  (mono)

class BufferSource : public AudioNode {
public:
    BufferSource(const std::vector<float>& data, size_t blockSize)
        : _data(data)
        , _blockSize(blockSize)
        , _outputBus(1, AudioBuffer(blockSize, 0.f))
    {
        _outputView[0] = AudioBufferView(_outputBus[0]);
    }

    bool done() const { return _pos >= _data.size(); }

    size_t numInputs()  const override { return 0; }
    size_t numOutputs() const override { return 1; }

    AudioBusView process(AudioBusView) override {
        const size_t remaining = _data.size() - _pos;
        const size_t toCopy    = std::min(_blockSize, remaining);

        std::copy(_data.begin() + _pos,
                  _data.begin() + _pos + toCopy,
                  _outputBus[0].begin());

        if (toCopy < _blockSize)
            std::fill(_outputBus[0].begin() + toCopy, _outputBus[0].end(), 0.f);

        _pos += toCopy;

        return AudioBusView(_outputView);
    }

private:
    const std::vector<float>&      _data;
    size_t                         _blockSize;
    size_t                         _pos = 0;
    AudioBus                       _outputBus;
    std::array<AudioBufferView, 1> _outputView {};
};
