#pragma once
#include <stdint.h>

struct IInstrument {
    virtual ~IInstrument() = default;
    virtual void noteOn(uint8_t pitch, uint8_t velocity) = 0;
    virtual void noteOff(uint8_t pitch) = 0;
};
