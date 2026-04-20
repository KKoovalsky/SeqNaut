#pragma once
#include <Arduino.h>

#include <chrono>

// Abstracts the source of wall-clock time consumed by MasterClock.
// Returning std::chrono::microseconds makes the unit explicit at the call site
// and allows mock implementations in tests.
struct ITimeProvider {
    virtual ~ITimeProvider() = default;
    virtual std::chrono::microseconds now() const = 0;
};

// Default implementation — delegates to Arduino's micros().
// micros() wraps at 2^32 (~71 min); MasterClock handles this with uint32_t
// unsigned subtraction, so wrapping is transparent.
struct ArduinoTimeProvider : ITimeProvider {
    std::chrono::microseconds now() const override {
        return std::chrono::microseconds(micros());
    }
};
