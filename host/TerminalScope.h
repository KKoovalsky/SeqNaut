#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "AudioNode.h"

// ── TerminalScope ─────────────────────────────────────────────────────────────
//
// Pass-through AudioNode that renders the signal as an ASCII oscilloscope,
// writing each frame to a dedicated named FIFO.  Read it in a separate
// terminal:
//
//   tail -f /tmp/seqnaut_scope_0
//
// Each instance gets a globally unique ID so multiple scopes never share a
// path.  The FIFO is opened O_RDWR so the constructor never blocks waiting
// for a reader, and writes are non-blocking so a missing or slow reader
// never stalls the audio path.
//
// numInputs()  = 1
// numOutputs() = 1  (pass-through)

class TerminalScope : public AudioNode {
public:
    explicit TerminalScope(size_t blockSize, std::string_view name, unsigned width = 120, unsigned height = 16)
        : _width(width),
          _height(height),
          _rows(height),
          _fifoPath("/tmp/seqnaut_scope_" + std::to_string(_nextId++)) {
        if (mkfifo(_fifoPath.c_str(), 0600) != 0 && errno != EEXIST)
            throw std::runtime_error("TerminalScope: mkfifo failed for " + _fifoPath);

        _fd = open(_fifoPath.c_str(), O_RDWR);
        if (_fd < 0)
            throw std::runtime_error("TerminalScope: open failed for " + _fifoPath);

        const int flags = fcntl(_fd, F_GETFL);
        fcntl(_fd, F_SETFL, flags | O_NONBLOCK);

        std::printf("[%.*s]  tail -f %s\n", static_cast<int>(name.size()), name.data(), _fifoPath.c_str());
    }

    ~TerminalScope() {
        if (_fd >= 0)
            close(_fd);
        unlink(_fifoPath.c_str());
    }

    TerminalScope(const TerminalScope&) = delete;
    TerminalScope& operator=(const TerminalScope&) = delete;

    const std::string& fifoPath() const { return _fifoPath; }

    size_t numInputs() const override { return 1; }
    size_t numOutputs() const override { return 0; }

    AudioBusView process(AudioBusView inputs) override {
        render(inputs[0]);
        return {};
    }

private:
    void render(AudioBufferView samples) {
        const unsigned centerRow = _height / 2;

        for (auto& row : _rows)
            row.assign(_width, ' ');
        for (unsigned c = 0; c < _width; ++c)
            _rows[centerRow][c] = '-';

        for (unsigned c = 0; c < _width && !samples.empty(); ++c) {
            const size_t sStart = (c * samples.size()) / _width;
            const size_t sEnd = std::max(sStart + 1, ((c + 1) * samples.size()) / _width);

            float lo = samples[sStart], hi = samples[sStart];
            for (size_t s = sStart + 1; s < sEnd && s < samples.size(); ++s) {
                lo = std::min(lo, samples[s]);
                hi = std::max(hi, samples[s]);
            }

            const unsigned rTop = sampleToRow(hi);
            const unsigned rBot = sampleToRow(lo);
            for (unsigned r = rTop; r <= rBot; ++r)
                _rows[r][c] = (r == centerRow) ? '+' : '|';
        }

        _frame.clear();
        if (!_firstFrame)
            _frame += "\033[" + std::to_string(_height) + "A";
        _firstFrame = false;

        for (const auto& row : _rows) {
            _frame += row;
            _frame += '\n';
        }

        write(_fd, _frame.data(), _frame.size());
    }

    unsigned sampleToRow(float v) const {
        v = std::clamp(v, -1.f, 1.f);
        return static_cast<unsigned>((1.f - v) * 0.5f * static_cast<float>(_height - 1) + 0.5f);
    }

    static std::atomic<int> _nextId;

    unsigned _width;
    unsigned _height;
    bool _firstFrame = true;
    int _fd = -1;
    std::string _fifoPath;
    std::vector<std::string> _rows;
    std::string _frame;  // reused each render() to avoid per-block allocation
};

inline std::atomic<int> TerminalScope::_nextId{0};
