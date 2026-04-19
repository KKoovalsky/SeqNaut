#pragma once
#include "AudioNode.h"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using ConnectionId = size_t;
class AudioConnection;

// ── Patch ─────────────────────────────────────────────────────────────────────
//
// Owns the audio graph topology and drives rendering.
//
// Nodes are discovered implicitly from connections — no explicit registration.
// A node with numInputs() == 0 is a source; a node with numOutputs() == 0 is
// a sink.  Both are treated identically by the graph.
//
// When a connection is added or removed, the graph immediately recomputes the
// topological order and pre-allocates all input view arrays.  process() then
// iterates the sorted list with no discovery, no traversal, and no state in
// nodes — it fills pre-allocated views and calls process() once per node.
//
// Usage:
//   Patch patch(128, 44100.f);
//
//   auto c = AudioConnection::from(oscillator)
//                            .to(filter).input(0)
//                            .in(patch)
//                            .connect();
//
//   patch.process();  // renders the full graph

class Patch {
public:
    Patch(size_t blockSize, float sampleRate)
        : _blockSize(blockSize)
        , _sampleRate(sampleRate)
    {
        _silence.assign(blockSize, 0.f);
    }

    float  sampleRate() const { return _sampleRate; }
    size_t blockSize()  const { return _blockSize; }

    // ── Graph execution ───────────────────────────────────────────────────────

    void process() {
        const AudioBufferView silenceView(_silence);

        for (size_t i = 0; i < _sorted.size(); ++i) {
            auto& entry = _sorted[i];

            for (size_t j = 0; j < entry.inputSources.size(); ++j) {
                const auto& src = entry.inputSources[j];
                entry.inputViews[j] = src.connected
                    ? _outputs[src.entryIdx][src.outputIdx]
                    : silenceView;
            }

            _outputs[i] = entry.node->process(AudioBusView(entry.inputViews));
        }
    }

private:

    friend class AudioConnection;

    ConnectionId addConnection(AudioNode& src, size_t srcOut,
                               AudioNode& dst, size_t dstIn) {
        if (srcOut >= src.numOutputs())
            throw std::out_of_range("Patch: srcOut exceeds numOutputs");
        if (dstIn >= dst.numInputs())
            throw std::out_of_range("Patch: dstIn exceeds numInputs");
        const ConnectionId id = _nextId++;
        _connections.push_back({ id, &src, srcOut, &dst, dstIn });
        rebuild();
        return id;
    }

    void removeConnection(ConnectionId id) {
        _connections.erase(
            std::remove_if(_connections.begin(), _connections.end(),
                [id](const ConnectionRecord& c) { return c.id == id; }),
            _connections.end());
        rebuild();
    }

    // ── Internal types ────────────────────────────────────────────────────────

    struct ConnectionRecord {
        ConnectionId id;
        AudioNode*   src;
        size_t       srcOut;
        AudioNode*   dst;
        size_t       dstIn;
    };

    struct InputSource {
        size_t entryIdx  = 0;
        size_t outputIdx = 0;
        bool   connected = false;
    };

    struct NodeEntry {
        AudioNode*                   node;
        std::vector<AudioBufferView> inputViews;    // pre-allocated; filled each process()
        std::vector<InputSource>     inputSources;  // pre-computed wiring
    };

    // ── State ─────────────────────────────────────────────────────────────────

    size_t _blockSize;
    float  _sampleRate;

    ConnectionId                  _nextId = 0;
    std::vector<ConnectionRecord> _connections;
    std::vector<NodeEntry>        _sorted;
    std::vector<AudioBusView>     _outputs;   // one per sorted entry; filled each process()
    AudioBuffer                   _silence;

    // ── Topology rebuild ──────────────────────────────────────────────────────
    //
    // Called whenever connections change.  Performs a topological sort
    // (Kahn's algorithm) and pre-allocates all input view arrays so that
    // process() stays allocation-free.

    void rebuild() {
        // Build in-degree map and adjacency sets.
        // 1. In-degree map stores for each key (the node - their addresses are unique) the number of nodes this specific
        //    node depends directly on. It's the number of nodes that must be processed before this one, but not all
        //    the nodes that must be processed, only the direct dependencies.
        // 2. Adjacency set contains connections from a specific node, to all the direct downstream nodes. It's a map
        //    that stores sets of nodes each that depend on that specific node. std::set gives node-level edge 
        //    deduplication for free.
        // 3. It might happen that one node has e.g. two outputs, and another node two inputs. When all the outputs 
        //    of the source node are connected to the same node, then it counts as one edge. Shortly: channel 
        //    connections between the same two nodes count as one edge.
        std::unordered_map<AudioNode*, int>                  inDegree;
        std::unordered_map<AudioNode*, std::set<AudioNode*>> adj;

        for (auto& c : _connections) {
            // std::unordered_map::try_emplace() will only emplace (add) an object to the map if it does't exist yet.
            inDegree.try_emplace(c.src, 0);
            inDegree.try_emplace(c.dst, 0);
            // Update the downstream set for the source node. In graph the connection is src -> dst.
            auto [_, isInserted] = adj[c.src].insert(c.dst);
            // For the destination node, if there was no connection found earlier for these nodes, we can safely
            // increase the in-degree count - the number of nodes the destination node depends on.
            if(isInserted)
                inDegree[c.dst]++;
        }

        // Kahn's topological sort
        // 1. Nodes with no dependencies are already sorted, and ready to be processed first.
        std::queue<AudioNode*> q;
        for (auto& [node, deg] : inDegree)
            if (deg == 0) q.push(node);

        // 2. Perform the actual sorting.
        std::vector<AudioNode*> sorted;
        while (!q.empty()) {
            auto* node = q.front(); q.pop();
            // 3. Nodes that are in the queue, have in-degree equal 0. This means that all the dependencies (the node
            //    that must be processed before) are already processed. We can push it to the sorted container.
            sorted.push_back(node);
            // 4. Iterate over each node that depends on the current node (iterating over the std::set).
            for (auto* dst : adj[node])
            {
                // 5. We have just processed (Ad.3 - above this loop) the node that this destination node depends on.
                //    We can safely decrease the in-degree counter for this destination node.
                auto &inDegreeDst{inDegree[dst]};
                --inDegreeDst;

                // 6. No dependencies left? Push to the queue.
                if (inDegreeDst == 0) 
                    q.push(dst);
            }
        }

        // 7. In case of a cycle: A → B → C → A   (cycle)
        //      - A depends on C, B depends on A, C depends on B
        //      - none of them ever reaches in-degree 0
        //      - none of them ever enters the queue
        //      - none of them ends up in sorted
        if (auto numNodes{inDegree.size()}; sorted.size() != numNodes)
            throw std::runtime_error("Patch: cycle detected in audio graph");

        // Build NodeEntry array — std::find replaces the nodeIdx map;
        // audio graphs are small so O(n²) is irrelevant here.
        _sorted.resize(sorted.size());
        for (size_t i = 0; i < sorted.size(); ++i) {
            auto& entry = _sorted[i];
            entry.node  = sorted[i];
            const size_t ni = sorted[i]->numInputs();
            entry.inputViews.resize(ni);
            entry.inputSources.assign(ni, {});
            for (auto& c : _connections) {
                if (c.dst == sorted[i] && c.dstIn < ni) {
                    auto it = std::find(sorted.begin(), sorted.end(), c.src);
                    entry.inputSources[c.dstIn] = {
                        static_cast<size_t>(std::distance(sorted.begin(), it)),
                        c.srcOut, true
                    };
                }
            }
        }

        _outputs.resize(sorted.size());
    }
};
