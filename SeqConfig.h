#pragma once

// ── Sequencer global constants ───────────────────────────────────────────────
// Change PPQN here to adjust the master clock resolution everywhere.
// Common values: 24 (MIDI minimum), 48 (practical for Teensy), 96 (DAW standard)

static constexpr uint8_t  PPQN           = 48;

// Pool sizes — increase if you need more tracks or notes
static constexpr uint8_t  MAX_CLOCKABLES = 8;
static constexpr uint8_t  MAX_TRACKS     = 8;
static constexpr uint8_t  MAX_STEPS      = 32;
static constexpr uint8_t  MAX_PR_NOTES   = 64;

// Handy note-value constants (ticks at the configured PPQN)
static constexpr uint8_t  TICKS_WHOLE        = PPQN * 4;
static constexpr uint8_t  TICKS_HALF         = PPQN * 2;
static constexpr uint8_t  TICKS_QUARTER      = PPQN;
static constexpr uint8_t  TICKS_EIGHTH       = PPQN / 2;
static constexpr uint8_t  TICKS_SIXTEENTH    = PPQN / 4;
static constexpr uint8_t  TICKS_THIRTYSECOND = PPQN / 8;
static constexpr uint8_t  TICKS_TRIPLET_8TH  = PPQN / 3;
static constexpr uint8_t  TICKS_DOTTED_8TH   = PPQN * 3 / 4;
