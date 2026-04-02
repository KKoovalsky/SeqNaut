#pragma once
#include <Arduino.h>
#include <chrono>
#include "SeqConfig.h"
#include "IClockable.h"
#include "ITimeProvider.h"

class MasterClock;

// ── ClockConnection ───────────────────────────────────────────────────────────
// RAII handle returned by MasterClock::connect().
// Deregisters the IClockable automatically on destruction.
// Move-only — ownership of the registration is transferred, never shared.
//
// Method bodies that call _clock->_remove() are defined after MasterClock
// is fully declared — an incomplete type cannot be dereferenced.
class ClockConnection {
public:
    ~ClockConnection();
    ClockConnection(ClockConnection&& o) noexcept;
    ClockConnection& operator=(ClockConnection&& o) noexcept;
    ClockConnection(const ClockConnection&)            = delete;
    ClockConnection& operator=(const ClockConnection&) = delete;

private:
    ClockConnection(MasterClock& clock, IClockable& clockable)
        : _clock(&clock), _clockable(&clockable) {}

    friend class MasterClock;
    MasterClock* _clock;
    IClockable*  _clockable;
};

// ── MasterClock ───────────────────────────────────────────────────────────────
// Single source of tempo. Drives all registered IClockables at PPQN-resolution
// ticks derived from the current BPM.
//
// Call update() from loop() — never from an ISR.
//
// TODO: replace update() with an ITimer interface so the clock can be driven
// by a hardware interrupt rather than requiring a manual call from loop().
class MasterClock {
public:
    explicit MasterClock(float bpm, ITimeProvider& time)
        : _time(time), _absoluteTick(0) {
        setBpm(bpm);
        _lastUs = static_cast<uint32_t>(_time.now().count());
    }

    void setBpm(float bpm) {
        _bpm        = bpm;
        _intervalUs = static_cast<uint32_t>(MICROS_PER_MINUTE / (bpm * PPQN));
    }

    float    bpm()          const { return _bpm; }
    uint32_t absoluteTick() const { return _absoluteTick; }

    // Register an IClockable and return a RAII connection handle.
    // Discarding the return value immediately deregisters — [[nodiscard]] makes
    // the compiler warn if the caller does so accidentally.
    [[nodiscard]] ClockConnection connect(IClockable& c) {
        _add(&c);
        return ClockConnection(*this, c);
    }

    // Call from loop() every iteration.
    void update() {
        // uint32_t subtraction is intentionally unsigned so micros() wrap-around
        // (~71 min) is handled correctly without branching.
        const auto nowUs     = static_cast<uint32_t>(_time.now().count());
        const auto elapsedUs = nowUs - _lastUs;
        if (elapsedUs < _intervalUs) return;

        _lastUs += _intervalUs;   // carry overshoot — prevents drift
        for (auto* c : _clockables) {
            if (c) c->tick(_absoluteTick);
        }
        ++_absoluteTick;
    }

private:
    void _add(IClockable* c) {
        for (auto*& slot : _clockables) {
            if (slot == nullptr) { slot = c; return; }
        }
    }

    void _remove(IClockable* c) {
        for (auto*& slot : _clockables) {
            if (slot == c) { slot = nullptr; return; }
        }
    }

    friend class ClockConnection;

    static constexpr float MICROS_PER_MINUTE = 60.0f * 1'000'000.0f;

    ITimeProvider& _time;
    float          _bpm          = 120.0f;
    uint32_t       _intervalUs   = 0;
    uint32_t       _lastUs       = 0;
    uint32_t       _absoluteTick = 0;
    IClockable*    _clockables[MAX_CLOCKABLES] = {};
};

// ── ClockConnection method bodies ─────────────────────────────────────────────
// Defined here — after MasterClock — so _clock->_remove() can be resolved.

inline ClockConnection::~ClockConnection() {
    if (_clock) _clock->_remove(_clockable);
}

inline ClockConnection::ClockConnection(ClockConnection&& o) noexcept
    : _clock(o._clock), _clockable(o._clockable) {
    o._clock     = nullptr;
    o._clockable = nullptr;
}

inline ClockConnection& ClockConnection::operator=(ClockConnection&& o) noexcept {
    if (this != &o) {
        if (_clock) _clock->_remove(_clockable);
        _clock       = o._clock;
        _clockable   = o._clockable;
        o._clock     = nullptr;
        o._clockable = nullptr;
    }
    return *this;
}
