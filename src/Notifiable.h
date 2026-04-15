#pragma once

// ── Notifiable<TEvent> ────────────────────────────────────────────────────────
//
// Minimal synchronous observer interface.
//
// Implementors receive events via notify().  Calls happen synchronously on
// whatever thread/context the notifier runs on — in the audio path that means
// the audio interrupt.  Implementations must therefore be real-time safe:
// no heap allocation, no blocking I/O, no locks.

template<typename TEvent>
class Notifiable {
public:
    virtual ~Notifiable() = default;
    virtual void notify(const TEvent&) = 0;
};
