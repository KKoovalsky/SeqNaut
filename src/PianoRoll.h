#pragma once
#include <stdint.h>

#include <variant>

#include "IClockable.h"
#include "IInstrument.h"
#include "MusicalTime.h"
#include "SeqConfig.h"

// PianoRoll — event-list sequencer driven by an external tick counter.
//
// Design notes:
//   - Stateless w.r.t. time: tick() derives position from absoluteTick directly.
//     No internal playhead — seeking, resetting, and syncing are free.
//   - NoteIds are stable: removing a note does not shift other ids.
//   - Instrument is injected separately from construction to support the OSC
//     workflow where pattern and instrument are created independently.

class PianoRoll : public IClockable {
public:
    using NoteId = uint8_t;

    enum class Error {
        OK,
        PoolFull,
        NoteNotFound,
    };

    explicit PianoRoll(MusicalTime length) : _length(length) {}

    void setInstrument(IInstrument& instrument) {
        _instrument = &instrument;
    }

    // Add a note. start is wrapped to [0, length).
    // Returns NoteId on success, Error otherwise.
    std::variant<NoteId, Error> addNote(MusicalTime start, MusicalTime duration, uint8_t pitch, uint8_t velocity = 100) {
        for (uint8_t i = 0; i < MAX_PR_NOTES; i++) {
            if (_notes[i]._active)
                continue;
            _notes[i] = {start % _length, duration, pitch, velocity, true};
            ++_noteCount;
            return NoteId(i);
        }
        return Error::PoolFull;
    }

    Error removeNote(NoteId id) {
        if (id >= MAX_PR_NOTES || !_notes[id]._active)
            return Error::NoteNotFound;
        _notes[id]._active = false;
        --_noteCount;
        return Error::OK;
    }

    void clear() {
        for (auto& n : _notes)
            n._active = false;
        _noteCount = 0;
    }

    uint8_t noteCount() const {
        return _noteCount;
    }

    MusicalTime length() const {
        return _length;
    }

    // TODO: optimise note lookup in tick().
    // Current approach is O(n) over the note pool regardless of how many notes
    // fire at this tick. Candidates:
    //
    //  1. Circular bucket array indexed by (absoluteTick % _length.count()):
    //     each bucket holds the events at that exact tick. tick() becomes an
    //     O(1) direct lookup; memory scales with pattern length, not note count.
    //     Fully preserves the stateless property.
    //
    //  2. Two sorted event lists (noteOn / noteOff sorted by tick position):
    //     advance a cursor forward on each call. O(1) when nothing fires,
    //     O(k) for k simultaneous events. Cursor is the only added state.
    //
    //  3. Intrusive linked list / timer wheel sorted by next-fire tick:
    //     classic O(1)-amortised event scheduler pattern.
    //
    // Stateless tick: position is derived from absoluteTick % length.
    // No internal counter — safe to call with any absoluteTick value.
    void tick(uint32_t absoluteTick) override {
        if (!_instrument || _noteCount == 0)
            return;

        const uint32_t local = absoluteTick % _length.count();
        uint8_t processed = 0;

        for (uint8_t i = 0; i < MAX_PR_NOTES && processed < _noteCount; i++) {
            if (!_notes[i]._active)
                continue;
            ++processed;
            const auto& n = _notes[i];

            if (local == n.start.count())
                _instrument->noteOn(n.pitch, n.velocity);

            const uint32_t offTick = (n.start + n.duration).count() % _length.count();
            if (local == offTick)
                _instrument->noteOff(n.pitch);
        }
    }

private:
    struct Note {
        MusicalTime start;
        MusicalTime duration;
        uint8_t pitch = 0;
        uint8_t velocity = 100;
        bool _active = false;
    };

    Note _notes[MAX_PR_NOTES] = {};
    uint8_t _noteCount = 0;
    MusicalTime _length;
    IInstrument* _instrument = nullptr;
};
