#pragma once
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <portaudio.h>

#include "AudioNode.h"

// ── AudioCardInput ───────────────────────────────────────────────────────────
//
// AudioNode source: captures audio from a hardware input device block by block.
// Uses PortAudio blocking I/O — process() calls Pa_ReadStream(), which blocks
// until the requested block of samples is ready.  This fits the pull model of
// Patch::process() without any separate capture thread or ring buffer.
//
// Device selection:
//   AudioCardInput src(block, rate);            // system default input
//   AudioCardInput src(block, rate, "Scarlett"); // first device matching substring
//   AudioCardInput src(block, rate, 3);          // device by index
//
// Stereo sources are downmixed to mono (L+R * 0.5).
// Devices with more than two input channels are capped at two for the downmix.
//
// Call AudioCardInput::listDevices() before constructing to discover available
// device names and indices.
//
// numInputs()  = 0  (source node)
// numOutputs() = 1  (mono)

class AudioCardInput : public AudioNode {
public:
    AudioCardInput(size_t blockSize, float sampleRate)
        : AudioCardInput(blockSize, sampleRate, defaultDeviceIndex()) {}

    AudioCardInput(size_t blockSize, float sampleRate, std::string_view nameSubstring)
        : AudioCardInput(blockSize, sampleRate, findDevice(nameSubstring)) {}

    AudioCardInput(size_t blockSize, float sampleRate, PaDeviceIndex device)
        : _blockSize(blockSize), _outputBus(1, AudioBuffer(blockSize, 0.f)) {
        _outputView[0] = AudioBufferView(_outputBus[0]);

        ensurePaInitialised();

        const PaDeviceInfo* info = Pa_GetDeviceInfo(device);
        if (!info)
            throw std::runtime_error("AudioCardInput: invalid device index " + std::to_string(device));
        if (info->maxInputChannels == 0)
            throw std::runtime_error("AudioCardInput: device '" + std::string(info->name) +
                                     "' has no input channels");

        _channels = static_cast<unsigned>(std::min(info->maxInputChannels, 2));

        PaStreamParameters params{};
        params.device = device;
        params.channelCount = static_cast<int>(_channels);
        params.sampleFormat = paFloat32 | paNonInterleaved;
        params.suggestedLatency = info->defaultLowInputLatency;

        // paNonInterleaved with _channels == 1: Pa_ReadStream writes directly
        // into _outputBus[0].  For stereo we read into _left/_right then downmix.
        if (_channels == 2) {
            _left.resize(blockSize);
            _right.resize(blockSize);
            _nonInterleavedPtrs[0] = _left.data();
            _nonInterleavedPtrs[1] = _right.data();
        }

        const PaError openErr = Pa_OpenStream(&_stream, &params, nullptr,
                                              static_cast<double>(sampleRate),
                                              static_cast<unsigned long>(blockSize),
                                              paClipOff, nullptr, nullptr);
        if (openErr != paNoError)
            throw std::runtime_error(std::string("AudioCardInput: Pa_OpenStream: ") +
                                     Pa_GetErrorText(openErr));

        const PaError startErr = Pa_StartStream(_stream);
        if (startErr != paNoError) {
            Pa_CloseStream(_stream);
            _stream = nullptr;
            throw std::runtime_error(std::string("AudioCardInput: Pa_StartStream: ") +
                                     Pa_GetErrorText(startErr));
        }
    }

    ~AudioCardInput() {
        if (_stream) {
            Pa_StopStream(_stream);
            Pa_CloseStream(_stream);
        }
    }

    AudioCardInput(const AudioCardInput&) = delete;
    AudioCardInput& operator=(const AudioCardInput&) = delete;

    size_t numInputs() const override { return 0; }
    size_t numOutputs() const override { return 1; }

    AudioBusView process(AudioBusView) override {
        if (_channels == 1) {
            // Non-interleaved mono: write straight into the output buffer.
            float* ptr = _outputBus[0].data();
            const PaError err = Pa_ReadStream(_stream, &ptr, static_cast<unsigned long>(_blockSize));
            if (err != paNoError && err != paInputOverflowed)
                std::fill(_outputBus[0].begin(), _outputBus[0].end(), 0.f);
        } else {
            const PaError err =
                Pa_ReadStream(_stream, _nonInterleavedPtrs.data(), static_cast<unsigned long>(_blockSize));
            if (err != paNoError && err != paInputOverflowed) {
                std::fill(_outputBus[0].begin(), _outputBus[0].end(), 0.f);
            } else {
                for (size_t i = 0; i < _blockSize; ++i)
                    _outputBus[0][i] = (_left[i] + _right[i]) * 0.5f;
            }
        }
        return AudioBusView(_outputView);
    }

    // Print all input-capable devices to stdout.
    static void listDevices() {
        ensurePaInitialised();
        const int n = Pa_GetDeviceCount();
        std::printf("Available input devices:\n");
        for (int i = 0; i < n; ++i) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (info && info->maxInputChannels > 0)
                std::printf("  [%2d] %s  (ch: %d)\n", i, info->name, info->maxInputChannels);
        }
    }

private:
    static void ensurePaInitialised() {
        static bool done = false;
        if (done)
            return;
        const PaError err = Pa_Initialize();
        if (err != paNoError)
            throw std::runtime_error(std::string("AudioCardInput: Pa_Initialize: ") +
                                     Pa_GetErrorText(err));
        done = true;
    }

    static PaDeviceIndex defaultDeviceIndex() {
        ensurePaInitialised();
        const PaDeviceIndex idx = Pa_GetDefaultInputDevice();
        if (idx == paNoDevice)
            throw std::runtime_error("AudioCardInput: no default input device");
        return idx;
    }

    static PaDeviceIndex findDevice(std::string_view substring) {
        ensurePaInitialised();
        const int n = Pa_GetDeviceCount();
        for (int i = 0; i < n; ++i) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (info && info->maxInputChannels > 0 &&
                std::string_view(info->name).find(substring) != std::string_view::npos)
                return static_cast<PaDeviceIndex>(i);
        }
        throw std::runtime_error("AudioCardInput: no input device matching '" +
                                 std::string(substring) + "'");
    }

    PaStream* _stream = nullptr;
    size_t _blockSize;
    unsigned _channels = 1;
    AudioBus _outputBus;
    std::array<AudioBufferView, 1> _outputView{};
    // Stereo non-interleaved buffers; unused for mono devices.
    std::vector<float> _left;
    std::vector<float> _right;
    std::array<void*, 2> _nonInterleavedPtrs{};
};
