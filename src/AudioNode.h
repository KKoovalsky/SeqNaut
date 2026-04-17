#pragma once
#include <cstddef>
#include <span>
#include <vector>

// ── Audio buffer types ────────────────────────────────────────────────────────
//
// AudioBuffer     — owns the sample data for one channel (heap-allocated once,
//                   reused every block)
// AudioBufferView — non-owning read-only view into one channel's samples
// AudioBus        — owns N AudioBuffers (one per channel)
// AudioBusView    — non-owning view over N AudioBufferViews; what nodes pass
//                   between each other without copying
//
// The asymmetry is intentional: nodes own their output buffers (AudioBus) and
// expose them as views (AudioBusView). Callers decide whether to copy.

using AudioBuffer     = std::vector<float>;
using AudioBufferView = std::span<const float>;
using AudioBus        = std::vector<AudioBuffer>;
using AudioBusView    = std::span<const AudioBufferView>;

// ── AudioNode ─────────────────────────────────────────────────────────────────
//
// Base interface for every node in the audio graph.
//
// Subclasses implement three methods only:
//   process(AudioBusView) — consume inputs, return output views
//   numInputs()           — number of input channels expected
//   numOutputs()          — number of output channels produced
//
// Wiring, traversal order, and output caching are entirely the responsibility
// of Patch. Nodes have no knowledge of the graph they live in.

class AudioNode {
public:
    virtual ~AudioNode() = default;

    virtual AudioBusView process(AudioBusView inputs) = 0;

    virtual size_t numInputs()  const = 0;
    virtual size_t numOutputs() const = 0;
};
