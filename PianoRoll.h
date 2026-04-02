#pragma once
#include <Arduino.h>
#include "SeqConfig.h"
#include "IClockable.h"

// Fired on noteOn / noteOff events.
// pitch    — MIDI note number
// velocity — 0-127
// on       — true = noteOn, false = noteOff
using NoteCallback = void (*)(uint8_t pitch, uint8_t velocity, bool on);

// ── PRNote ────────────────────────────────────────────────────────────────────
struct PRNote {
    uint32_t startTick = 0;
    uint32_t duration  = TICKS_EIGHTH;
    uint8_t  pitch     = 60;
    uint8_t  velocity  = 100;

    // Internal — reset on every pattern loop
    bool _onFired  = false;
    bool _offFired = false;
};

// ── PianoRoll ─────────────────────────────────────────────────────────────────
// Event-list sequencer. Notes are placed at arbitrary tick positions giving
// continuous resolution up to the master clock granularity (1 tick).
// The pattern loops automatically when the local tick reaches lengthTicks.
class PianoRoll : public IClockable {
public:
    // lengthTicks — pattern duration before looping.
    // Example: 2 bars = 2 * TICKS_WHOLE  (384 ticks at 48 PPQN)
    explicit PianoRoll(uint32_t lengthTicks = 2 * TICKS_WHOLE)
        : _length(lengthTicks) {}

    // Add a note. startTick is wrapped to [0, length).
    // Returns note index, or -1 if pool is full.
    int8_t addNote(uint32_t startTick,
                   uint32_t duration,
                   uint8_t  pitch,
                   uint8_t  velocity = 100) {
        if (_count >= MAX_PR_NOTES) return -1;
        auto& n     = _notes[_count];
        n.startTick = startTick % _length;
        n.duration  = duration;
        n.pitch     = pitch;
        n.velocity  = velocity;
        n._onFired  = false;
        n._offFired = false;
        return static_cast<int8_t>(_count++);
    }

    void removeNote(uint8_t idx) {
        if (idx >= _count) return;
        for (uint8_t i = idx; i < _count - 1; i++) _notes[i] = _notes[i + 1];
        --_count;
    }

    void clear() {
        _count     = 0;
        _localTick = 0;
    }

    uint8_t  noteCount() const { return _count; }
    uint32_t length()    const { return _length; }
    uint32_t localTick() const { return _localTick; }

    void onNote(NoteCallback cb) { _cb = cb; }

    void tick(uint32_t /*absoluteTick*/) override {
        for (uint8_t i = 0; i < _count; i++) {
            auto& n = _notes[i];

            if (!n._onFired && _localTick == n.startTick) {
                if (_cb) _cb(n.pitch, n.velocity, true);
                n._onFired = true;
            }

            const uint32_t offTick = (n.startTick + n.duration) % _length;
            if (!n._offFired && _localTick == offTick) {
                if (_cb) _cb(n.pitch, n.velocity, false);
                n._offFired = true;
            }
        }

        if (++_localTick >= _length) {
            _localTick = 0;
            for (uint8_t i = 0; i < _count; i++) {
                _notes[i]._onFired  = false;
                _notes[i]._offFired = false;
            }
        }
    }

private:
    PRNote       _notes[MAX_PR_NOTES] = {};
    uint8_t      _count     = 0;
    uint32_t     _length;
    uint32_t     _localTick = 0;
    NoteCallback _cb        = nullptr;
};
