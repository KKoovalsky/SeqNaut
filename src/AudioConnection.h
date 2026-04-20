#pragma once
#include "Patch.h"

// ── AudioConnection ───────────────────────────────────────────────────────────
//
// RAII handle representing one wired channel connection inside a Patch.
// Move-only, [[nodiscard]].  The connection is live for the lifetime of the
// object and severed — and the Patch topology rebuilt — on destruction.
//
// Constructed exclusively through the builder:
//
//   auto c = AudioConnection::from(oscillator)
//                            .output(0)
//                            .to(filter)
//                            .input(0)
//                            .in(patch)
//                            .connect();
//
// output() and input() default to 0, so for single-channel nodes:
//
//   auto c = AudioConnection::from(src).to(dst).in(patch).connect();

class [[nodiscard]] AudioConnection {
public:
    // ── Builder ───────────────────────────────────────────────────────────────
    //
    // TODO: Minimise rebuild() invocations when wiring multiple connections at
    //   once.  Each connect() call currently triggers a full rebuild().  A
    //   better approach: disallow Builder as an lvalue (delete copy/assign,
    //   make chaining methods &&-qualified so they are only callable on
    //   temporaries), collect all pending ConnectionRecords while the builder
    //   is alive, and commit them all — triggering a single rebuild() — either
    //   on an explicit flush() call or on destruction.  A PatchBuilder wrapper
    //   (rather than extending AudioConnection::Builder) may be a cleaner fit.

    class Builder {
    public:
        Builder& output(size_t idx) {
            _srcOut = idx;
            return *this;
        }
        Builder& to(AudioNode& dst) {
            _dst = &dst;
            return *this;
        }
        Builder& input(size_t idx) {
            _dstIn = idx;
            return *this;
        }
        Builder& in(Patch& patch) {
            _patch = &patch;
            return *this;
        }

        [[nodiscard]] AudioConnection connect() {
            return AudioConnection(Key{}, *_patch, _patch->addConnection(*_src, _srcOut, *_dst, _dstIn));
        }

    private:
        friend class AudioConnection;
        explicit Builder(AudioNode& src) : _src(&src) {}

        AudioNode* _src = nullptr;
        AudioNode* _dst = nullptr;
        size_t _srcOut = 0;
        size_t _dstIn = 0;
        Patch* _patch = nullptr;
    };

    static Builder from(AudioNode& src) {
        return Builder(src);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    ~AudioConnection() {
        if (_patch)
            _patch->removeConnection(_id);
    }

    AudioConnection(const AudioConnection&) = delete;
    AudioConnection& operator=(const AudioConnection&) = delete;

    AudioConnection(AudioConnection&& o) noexcept : _patch(o._patch), _id(o._id) {
        o._patch = nullptr;
    }

    AudioConnection& operator=(AudioConnection&& o) noexcept {
        if (this != &o) {
            if (_patch)
                _patch->removeConnection(_id);
            _patch = o._patch;
            _id = o._id;
            o._patch = nullptr;
        }
        return *this;
    }

private:
    struct Key {
        explicit Key() = default;
    };

    AudioConnection(Key, Patch& patch, ConnectionId id) : _patch(&patch), _id(id) {}

    Patch* _patch;
    ConnectionId _id;
};
