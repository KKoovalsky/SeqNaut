// SeqnautDemo — Phase 2: MasterClock + StepGrid
// Teensy 4.1 + PJRC Audio Shield (SGTL5000)
//
// Serial commands (921600 baud, newline-terminated):
//   b<n>   set BPM   e.g. "b130"
//   ?      status

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

#include "SeqConfig.h"
#include "IClockable.h"
#include "ITimeProvider.h"
#include "MasterClock.h"
#include "StepGrid.h"
#include "PianoRoll.h"

// ── Audio graph ──────────────────────────────────────────────────────────────
AudioSynthWaveform   osc;
AudioEffectEnvelope  env;
AudioMixer4          mix;
AudioOutputI2S       out;
AudioControlSGTL5000 codec;

AudioConnection c1(osc, 0, env, 0);
AudioConnection c2(env, 0, mix, 0);
AudioConnection c3(mix, 0, out, 0);
AudioConnection c4(mix, 0, out, 1);

// ── Sequencer ────────────────────────────────────────────────────────────────
ArduinoTimeProvider timeProvider;
MasterClock         clock(120.0f, timeProvider);
StepGrid            grid;

// Root MIDI note per track
static const uint8_t trackNote[] = { 60, 64, 67, 55 };   // C4 E4 G4 G3

static float midiToHz(uint8_t note) {
    return 440.0f * powf(2.0f, static_cast<float>(static_cast<int8_t>(note - 69)) / 12.0f);
}

static void onStep(uint8_t trackIdx, uint8_t /*stepIdx*/, uint8_t velocity) {
    if (trackIdx < sizeof(trackNote)) {
        osc.frequency(midiToHz(trackNote[trackIdx]));
        osc.amplitude(velocity / 127.0f);
        env.noteOn();
    }
}

// ── Serial ───────────────────────────────────────────────────────────────────
static char    serialBuf[32];
static uint8_t serialPos = 0;

static void handleCommand(const char* cmd) {
    if (cmd[0] == 'b') {
        const float bpm = atof(cmd + 1);
        if (bpm > 10.0f && bpm < 400.0f) {
            clock.setBpm(bpm);
            Serial.printf(">> BPM=%.1f\n", bpm);
        }
    } else if (cmd[0] == '?') {
        Serial.printf("BPM=%.1f  tick=%lu  tracks=%u\n",
                      clock.bpm(), clock.absoluteTick(), grid.trackCount());
        Serial.printf("AudioMemory peak: %u  CPU peak: %.1f%%\n",
                      AudioMemoryUsageMax(), AudioProcessorUsageMax());
    }
}

static void pollSerial() {
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (serialPos > 0) {
                serialBuf[serialPos] = '\0';
                handleCommand(serialBuf);
                serialPos = 0;
            }
        } else if (serialPos < sizeof(serialBuf) - 1) {
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

    // ── Build the step grid ──────────────────────────────────────────────────
    // Four tracks at 8th-note resolution; different lengths create polyrhythm.
    const int8_t t0 = grid.addTrack("Root",  16, TICKS_EIGHTH);
    const int8_t t1 = grid.addTrack("Third", 12, TICKS_EIGHTH);
    const int8_t t2 = grid.addTrack("Fifth", 16, TICKS_EIGHTH);
    const int8_t t3 = grid.addTrack("Low",    8, TICKS_EIGHTH);

    if (t0 >= 0) {
        auto* t = grid.track(t0);
        t->set(0, true);      t->set(4,  true, 80);
        t->set(8, true);      t->set(10, true, 90);
        t->set(12, true, 70);
    }
    if (t1 >= 0) {
        auto* t = grid.track(t1);
        t->set(0, true);  t->set(3, true, 85);
        t->set(6, true);  t->set(9, true, 75);
    }
    if (t2 >= 0) {
        auto* t = grid.track(t2);
        t->set(2,  true, 90);  t->set(6,  true);
        t->set(10, true, 80);  t->set(14, true, 95);
    }
    if (t3 >= 0) {
        auto* t = grid.track(t3);
        t->set(0, true);  t->set(4, true, 85);  t->set(6, true, 70);
    }

    grid.onFire(onStep);

    // connect() is [[nodiscard]] — store the connection so it stays registered
    // for the lifetime of the program.
    static auto gridConn = clock.connect(grid);

    Serial.printf("SeqnautDemo ready — PPQN=%u  BPM=%.0f\n", PPQN, clock.bpm());
    Serial.println("Commands: b<bpm> | ?");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    clock.update();
    pollSerial();
}
