#pragma once
#include <Audio.h>

#include <memory>
#include <vector>

#include "AudioNode.h"

// ── AudioNodeStream ───────────────────────────────────────────────────────────
//
// Adapter: AudioNode → AudioStream.
//
// Bridges the platform-agnostic AudioNode graph into the Teensy Audio library's
// AudioStream graph.  Wire it with AudioConnection like any other AudioStream:
//
//   SomeAudioNode        node;
//   AudioNodeStream      stream(node);
//   AudioOutputI2S       out;
//   AudioConnection      c(stream, 0, out, 0);
//
// All heap allocation happens at construction.  update() is allocation-free and
// safe to run from the audio interrupt.
//
// int16 ↔ float conversion:
//   Teensy Audio uses Q15 int16 (±32767).  DaisySP outputs ±1.0 float.
//   input:  sample = int16 * (1 / 32768)
//   output: int16  = float * 32767
//
// If an input channel has no AudioConnection, it is filled with silence.
// If allocate() fails (pool exhausted), that output channel is skipped.

// ── AudioQueueHolder ──────────────────────────────────────────────────────────
//
// Owns the audio_block_t* input queue array required by AudioStream's
// constructor.  Declared as a private base so it is fully constructed before
// AudioStream's constructor runs, giving AudioStream a valid pointer.

struct AudioQueueHolder {
    explicit AudioQueueHolder(size_t n) : queue(std::make_unique<audio_block_t*[]>(n)) {}
    std::unique_ptr<audio_block_t*[]> queue;
};

class AudioNodeStream : private AudioQueueHolder, public AudioStream {
public:
    explicit AudioNodeStream(AudioNode& node)
        : AudioQueueHolder(node.numInputs()),
          AudioStream(node.numInputs(), AudioQueueHolder::queue.get()),
          _node(node),
          _floatIn(node.numInputs(), std::vector<float>(AUDIO_BLOCK_SAMPLES)),
          _inBlocks(node.numInputs(), nullptr),
          _inViews(node.numInputs()) {}

    void update() override {
        // 1. Receive inputs, convert int16 → float
        for (size_t ch = 0; ch < _node.numInputs(); ++ch) {
            _inBlocks[ch] = receiveReadOnly(ch);
            if (_inBlocks[ch]) {
                for (size_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
                    _floatIn[ch][i] = _inBlocks[ch]->data[i] * (1.f / 32768.f);
            } else {
                std::fill(_floatIn[ch].begin(), _floatIn[ch].end(), 0.f);
            }
            _inViews[ch] = AudioBufferView(_floatIn[ch]);
        }

        // 2. Process
        const AudioBusView outputBus = _node.process(AudioBusView(_inViews));

        // 3. Convert float → int16, transmit
        for (size_t ch = 0; ch < _node.numOutputs(); ++ch) {
            audio_block_t* outBlock = allocate();
            if (outBlock) {
                for (size_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
                    outBlock->data[i] = static_cast<int16_t>(outputBus[ch][i] * 32767.f);
                transmit(outBlock, ch);
                release(outBlock);
            }
        }

        // 4. Release input blocks
        for (size_t ch = 0; ch < _node.numInputs(); ++ch)
            if (_inBlocks[ch])
                release(_inBlocks[ch]);
    }

private:
    AudioNode& _node;
    std::vector<std::vector<float>> _floatIn;
    std::vector<audio_block_t*> _inBlocks;
    std::vector<AudioBufferView> _inViews;
};
