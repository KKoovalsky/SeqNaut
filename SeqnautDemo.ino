// SeqnautDemo — Phase 2: pattern sequencer with chaining and serial control
// Teensy 4.1 + PJRC Audio Shield (SGTL5000)
//
// Serial commands (921600 baud, newline-terminated):
//   p<n>   queue pattern switch at next pattern boundary  (e.g. "p2")
//   P<n>   switch pattern immediately                     (e.g. "P0")
//   b<n>   set global BPM                                 (e.g. "b130")
//   g<n>   set global gate percent                        (e.g. "g80")
//   c      toggle chain mode on/off
//   ?      print status + audio memory/CPU usage

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// ── Audio graph ──────────────────────────────────────────────────────────────
AudioSynthWaveform   osc;
AudioEffectEnvelope  env;
AudioMixer4          mix;
AudioOutputI2S       out;
AudioControlSGTL5000 codec;

AudioConnection c1(osc, 0, env, 0);
AudioConnection c2(env, 0, mix, 0);
AudioConnection c3(mix, 0, out, 0);   // left
AudioConnection c4(mix, 0, out, 1);   // right

// ── Pattern data ─────────────────────────────────────────────────────────────
static constexpr uint8_t MAX_STEPS = 16;

struct Pattern {
    const char* name;
    uint8_t     length;             // active steps (1..MAX_STEPS)
    uint8_t     notes[MAX_STEPS];   // MIDI note number; 0 = rest
    float       bpm;                // 0 = inherit globalBpm
    float       gate;               // 0 = inherit globalGate
};

static const Pattern patterns[] = {
    // 0 — ascending arpeggio in C minor pentatonic
    { "Arpeggio", 16,
      { 60,  0, 63,  0,
        67,  0, 63, 62,
        60,  0, 55,  0,
        58, 60, 63,  0 },
      120.0f, 0.75f },

    // 1 — bass line, slower, sparse
    { "Bass", 16,
      { 36,  0,  0,  0,
        36,  0, 38,  0,
        41,  0,  0,  0,
        43,  0, 41, 39 },
      90.0f, 0.60f },

    // 2 — descending melody, busier feel
    { "Melody", 16,
      { 72, 71, 70, 67,
        65, 63, 62, 60,
        62,  0, 63,  0,
        65,  0, 67,  0 },
      130.0f, 0.80f },

    // 3 — short sparse phrase (8 steps only)
    { "Sparse", 8,
      { 48,  0,  0,  0,
        55,  0,  0, 52 },
      100.0f, 0.50f },
};

static constexpr uint8_t NUM_PATTERNS = sizeof(patterns) / sizeof(patterns[0]);

// ── Chain ─────────────────────────────────────────────────────────────────────
// Each entry: which pattern to play and how many times (0 = loop forever).
struct ChainEntry {
    uint8_t patternIdx;
    uint8_t repeats;
};

static const ChainEntry chain[] = {
    { 0, 2 },
    { 2, 1 },
    { 1, 2 },
    { 3, 1 },
};
static constexpr uint8_t CHAIN_LEN = sizeof(chain) / sizeof(chain[0]);

// ── Runtime state ─────────────────────────────────────────────────────────────
static float   globalBpm  = 120.0f;
static float   globalGate = 0.75f;

static uint8_t currentPatternIdx = 0;
static uint8_t currentStep       = 0;
static uint8_t patternPlayCount  = 0;   // completed plays of the current pattern
static uint8_t chainPos          = 0;
static bool    chainMode         = true;
static bool    noteActive        = false;
static uint8_t pendingPattern    = 0xFF;   // 0xFF = no pending switch

elapsedMillis stepTimer;
uint32_t      stepDuration;   // ms per 16th-note step
uint32_t      gateDuration;   // ms the gate stays open

// ── Helpers ───────────────────────────────────────────────────────────────────
static float midiToHz(uint8_t note) {
    return 440.0f * powf(2.0f, (int8_t)(note - 69) / 12.0f);
}

static void recalcTiming() {
    const Pattern& p = patterns[currentPatternIdx];
    float bpm  = (p.bpm  > 0.0f) ? p.bpm  : globalBpm;
    float gate = (p.gate > 0.0f) ? p.gate : globalGate;
    stepDuration = (uint32_t)(15000.0f / bpm);   // 60000 / (bpm * 4) — 16th notes
    gateDuration = (uint32_t)(stepDuration * gate);
}

static void switchPattern(uint8_t idx) {
    currentPatternIdx = idx;
    currentStep       = 0;
    patternPlayCount  = 0;
    recalcTiming();
    const Pattern& p = patterns[idx];
    float bpm = (p.bpm > 0.0f) ? p.bpm : globalBpm;
    Serial.printf(">> [%u] %s  BPM=%.0f  step=%ums  gate=%ums\n",
                  idx, p.name, bpm, stepDuration, gateDuration);
}

static void triggerStep() {
    uint8_t note = patterns[currentPatternIdx].notes[currentStep];

    if (note > 0) {
        osc.frequency(midiToHz(note));
        env.noteOn();
        noteActive = true;
        Serial.printf("  s%02u  n%3u  %.1fHz\n", currentStep, note, midiToHz(note));
    } else {
        if (noteActive) { env.noteOff(); noteActive = false; }
        Serial.printf("  s%02u  rest\n", currentStep);
    }
}

// Called at every pattern boundary to decide whether to advance the chain.
static void advanceChain() {
    const ChainEntry& entry = chain[chainPos];
    // repeats == 0 means loop this pattern forever
    if (entry.repeats == 0 || patternPlayCount < entry.repeats) return;

    chainPos = (chainPos + 1) % CHAIN_LEN;
    switchPattern(chain[chainPos].patternIdx);
}

// ── Serial command parser ─────────────────────────────────────────────────────
static char    serialBuf[32];
static uint8_t serialPos = 0;

static void handleCommand(const char* cmd) {
    if (cmd[0] == 'p' || cmd[0] == 'P') {
        uint8_t idx = (uint8_t)atoi(cmd + 1);
        if (idx >= NUM_PATTERNS) {
            Serial.printf("! valid range: 0..%u\n", NUM_PATTERNS - 1);
            return;
        }
        if (cmd[0] == 'P') {
            pendingPattern = 0xFF;
            if (noteActive) { env.noteOff(); noteActive = false; }
            switchPattern(idx);
            stepTimer = 0;
            triggerStep();
        } else {
            pendingPattern = idx;
            Serial.printf(">> pattern %u queued\n", idx);
        }

    } else if (cmd[0] == 'b') {
        float bpm = atof(cmd + 1);
        if (bpm > 10.0f && bpm < 400.0f) {
            globalBpm = bpm;
            recalcTiming();
            Serial.printf(">> BPM=%.1f  step=%ums\n", globalBpm, stepDuration);
        }

    } else if (cmd[0] == 'g') {
        float g = atof(cmd + 1) / 100.0f;
        if (g > 0.0f && g <= 1.0f) {
            globalGate = g;
            recalcTiming();
            Serial.printf(">> gate=%.0f%%  gate=%ums\n", globalGate * 100.0f, gateDuration);
        }

    } else if (cmd[0] == 'c') {
        chainMode = !chainMode;
        Serial.printf(">> chain %s\n", chainMode ? "ON" : "OFF");

    } else if (cmd[0] == '?') {
        const Pattern& p   = patterns[currentPatternIdx];
        float          bpm = (p.bpm > 0.0f) ? p.bpm : globalBpm;
        Serial.printf("Pattern [%u] %s  step %u/%u  play %u  chain[%u] %s  BPM=%.0f\n",
                      currentPatternIdx, p.name,
                      currentStep, p.length - 1,
                      patternPlayCount,
                      chainPos, chainMode ? "ON" : "OFF",
                      bpm);
        Serial.printf("Memory peak: %u blocks  CPU peak: %.1f%%\n",
                      AudioMemoryUsageMax(), AudioProcessorUsageMax());

    } else {
        Serial.println("! unknown command");
    }
}

static void pollSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialPos > 0) {
                serialBuf[serialPos] = '\0';
                handleCommand(serialBuf);
                serialPos = 0;
            }
        } else if (serialPos < (uint8_t)(sizeof(serialBuf) - 1)) {
            serialBuf[serialPos++] = c;
        }
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(921600);

    AudioMemory(16);

    codec.enable();
    codec.volume(0.7f);

    osc.begin(WAVEFORM_SAWTOOTH);
    osc.amplitude(0.8f);
    osc.frequency(440.0f);

    env.attack(6.0f);
    env.decay(40.0f);
    env.sustain(0.55f);
    env.release(50.0f);

    mix.gain(0, 0.6f);

    switchPattern(chain[0].patternIdx);
    stepTimer = 0;
    triggerStep();

    Serial.println("Commands: p<n> queue | P<n> immediate | b<bpm> | g<gate%> | c chain | ?");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    pollSerial();

    // Close gate at the note's fraction of the step
    if (noteActive && stepTimer >= gateDuration) {
        env.noteOff();
        noteActive = false;
    }

    // Advance to next step when the full step has elapsed
    if (stepTimer >= stepDuration) {
        stepTimer -= stepDuration;   // carry overshoot into next step
        currentStep++;

        if (currentStep >= patterns[currentPatternIdx].length) {
            // Pattern boundary
            currentStep = 0;
            patternPlayCount++;

            if (pendingPattern != 0xFF) {
                // Manual queued switch takes priority over chain
                switchPattern(pendingPattern);
                pendingPattern = 0xFF;
            } else if (chainMode) {
                advanceChain();
            }
        }

        triggerStep();
    }
}
