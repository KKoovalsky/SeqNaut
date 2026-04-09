#pragma once
#include <stdint.h>

// ── Sequencer global constants ───────────────────────────────────────────────
// Change PPQN here to adjust the master clock resolution everywhere.
// Common values: 24 (MIDI minimum), 48 (practical for Teensy), 96 (DAW standard)
static constexpr uint32_t PPQN = 48;

// Pool sizes — increase if you need more of each
static constexpr uint8_t MAX_CLOCKABLES = 8;
static constexpr uint8_t MAX_TRACKS     = 8;
static constexpr uint8_t MAX_STEPS      = 32;
static constexpr uint8_t MAX_PR_NOTES   = 64;
