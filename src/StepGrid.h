#pragma once
#include <stdint.h>
#include <string.h>

#include "IClockable.h"
#include "IInstrument.h"
#include "MusicalTime.h"
#include "SeqConfig.h"

// Fired when an active step is reached.
// trackIdx  — slot index returned by addTrack()
// stepIdx   — step within that track (0..length-1)
// velocity  — 0-127
using StepFireCallback = void (*)(uint8_t trackIdx, uint8_t stepIdx, uint8_t velocity);

// ── GridStep ──────────────────────────────────────────────────────────────────
struct GridStep {
    bool active = false;
    uint8_t velocity = 100;
};

// ── GridTrack ─────────────────────────────────────────────────────────────────
struct GridTrack {
    char name[16] = {};
    uint8_t length = 16;
    MusicalTime ticksPerStep = MusicalTime::beats(1);
    GridStep steps[MAX_STEPS] = {};

    bool _inUse = false;
    uint8_t _currentStep = 0;

    void set(uint8_t idx, bool active, uint8_t velocity = 100) {
        if (idx < MAX_STEPS) {
            steps[idx].active = active;
            steps[idx].velocity = velocity;
        }
    }

    void clear() {
        for (auto& s : steps)
            s = {};
        _currentStep = 0;
    }
};

// ── StepGrid ──────────────────────────────────────────────────────────────────
// Starts completely empty. Tracks are added dynamically from a fixed pool.
// Each track has its own ticksPerStep and length — polyrhythm falls out
// naturally from different combinations.
//
// Firing is aligned to absoluteTick:
//   track fires when  absoluteTick % ticksPerStep.count() == 0
class StepGrid : public IClockable {
public:
    StepGrid() = default;

    // Returns slot index (0..MAX_TRACKS-1), or -1 if pool is full.
    int8_t addTrack(const char* name, uint8_t length = 16, MusicalTime ticksPerStep = MusicalTime::beats(1)) {
        for (uint8_t i = 0; i < MAX_TRACKS; i++) {
            if (_tracks[i]._inUse)
                continue;
            auto& t = _tracks[i];
            t = {};
            strncpy(t.name, name, sizeof(t.name) - 1);
            t.length = length;
            t.ticksPerStep = ticksPerStep;
            t._inUse = true;
            return static_cast<int8_t>(i);
        }
        return -1;
    }

    void removeTrack(uint8_t idx) {
        if (idx < MAX_TRACKS)
            _tracks[idx]._inUse = false;
    }

    GridTrack* track(uint8_t idx) {
        if (idx >= MAX_TRACKS || !_tracks[idx]._inUse)
            return nullptr;
        return &_tracks[idx];
    }

    uint8_t trackCount() const {
        uint8_t n = 0;
        for (const auto& t : _tracks)
            if (t._inUse)
                ++n;
        return n;
    }

    void onFire(StepFireCallback cb) {
        _cb = cb;
    }

    void tick(uint32_t absoluteTick) override {
        for (uint8_t i = 0; i < MAX_TRACKS; i++) {
            auto& t = _tracks[i];
            if (!t._inUse)
                continue;
            if (absoluteTick % t.ticksPerStep.count() != 0)
                continue;

            const uint8_t step = t._currentStep;
            t._currentStep = (t._currentStep + 1) % t.length;

            if (_cb && t.steps[step].active)
                _cb(i, step, t.steps[step].velocity);
        }
    }

private:
    GridTrack _tracks[MAX_TRACKS] = {};
    StepFireCallback _cb = nullptr;
};
