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
// Inputs arrive as a non-owning AudioBusView — stable pointers into upstream
// nodes' own output buffers. A node must never hold onto the view beyond its
// process() call, as upstream buffers will be overwritten on the next block.
//
// The return value is a non-owning view into the node's own internal output
// buffers, which remain valid until the next process() call on this node.
// Downstream nodes read from it; the graph must not call process() on this
// node again until all consumers have finished reading.
class AudioNode {
public:
    virtual ~AudioNode() = default;

    virtual AudioBusView process(AudioBusView inputs) = 0;

    virtual size_t numInputs()  const = 0;
    virtual size_t numOutputs() const = 0;
};
