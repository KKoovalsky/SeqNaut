#pragma once
#include <stdint.h>

// Anything driven by the MasterClock implements this interface.
class IClockable {
public:
    virtual ~IClockable() = default;
    virtual void tick(uint32_t absoluteTick) = 0;
};
