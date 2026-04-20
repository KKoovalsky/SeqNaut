#pragma once
#include <stdint.h>

#include "SeqConfig.h"

// Represents a position or duration in musical time.
// The internal unit is ticks (1/PPQN of a quarter note).
// BPM is intentionally absent — conversion to real time lives in MasterClock only.
//
// Construction is only possible via the named factory methods, keeping the
// unit explicit at every call site:
//
//   MusicalTime::beats(2)       — 2 quarter notes
//   MusicalTime::ticks(6)       — 6 raw PPQN ticks
//
// bars() is omitted until a TimeSignature type exists to make it unambiguous.

struct MusicalTime {
    constexpr MusicalTime() : _ticks(0) {}

    constexpr uint32_t count() const {
        return _ticks;
    }

    static constexpr MusicalTime beats(uint32_t n) {
        return MusicalTime(n * PPQN);
    }
    static constexpr MusicalTime ticks(uint32_t n) {
        return MusicalTime(n);
    }

    constexpr bool operator==(const MusicalTime& o) const {
        return _ticks == o._ticks;
    }
    constexpr bool operator!=(const MusicalTime& o) const {
        return _ticks != o._ticks;
    }
    constexpr bool operator<(const MusicalTime& o) const {
        return _ticks < o._ticks;
    }
    constexpr bool operator>=(const MusicalTime& o) const {
        return _ticks >= o._ticks;
    }

    constexpr MusicalTime operator+(const MusicalTime& o) const {
        return MusicalTime(_ticks + o._ticks);
    }
    constexpr MusicalTime operator%(const MusicalTime& o) const {
        return MusicalTime(_ticks % o._ticks);
    }
    constexpr MusicalTime operator*(uint32_t scalar) const {
        return MusicalTime(_ticks * scalar);
    }

private:
    constexpr explicit MusicalTime(uint32_t t) : _ticks(t) {}
    uint32_t _ticks;
};
