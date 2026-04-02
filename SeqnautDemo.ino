// SeqnautDemo — PianoRoll + SawtoothVoice
// Teensy 4.1 + PJRC Audio Shield (SGTL5000)

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

#include "SeqConfig.h"
#include "MusicalTime.h"
#include "IClockable.h"
#include "IInstrument.h"
#include "ITimeProvider.h"
#include "MasterClock.h"
#include "PianoRoll.h"
#include "SawtoothVoice.h"

// ── Audio graph ──────────────────────────────────────────────────────────────
SawtoothVoice        voice;
AudioMixer4          mixer;
AudioOutputI2S       out;
AudioControlSGTL5000 codec;

AudioConnection voiceToMixer(voice,  0, mixer, 0);
AudioConnection mixerToLeft (mixer,  0, out,   0);
AudioConnection mixerToRight(mixer,  0, out,   1);

// ── Sequencer ────────────────────────────────────────────────────────────────
ArduinoTimeProvider timeProvider;
MasterClock         masterClock(120.0f, timeProvider);
PianoRoll           roll(MusicalTime::beats(8));   // 2-bar pattern

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(921600);

    AudioMemory(8);
    codec.enable();
    codec.volume(0.7f);

    mixer.gain(0, 0.8f);

    // ── A minor pentatonic melody, 2 bars ────────────────────────────────────
    // Scale: A4(69) C5(72) D5(74) E5(76) G5(79)
    //
    //  Bar 1: A4  C5  D5  E5
    //  Bar 2: D5  C5  A4  (rest)

    const auto q = MusicalTime::beats(1);

    roll.addNote(q * 0, q, 69);   // A4
    roll.addNote(q * 1, q, 72);   // C5
    roll.addNote(q * 2, q, 74);   // D5
    roll.addNote(q * 3, q, 76);   // E5
    roll.addNote(q * 4, q, 74);   // D5
    roll.addNote(q * 5, q, 72);   // C5
    roll.addNote(q * 6, q, 69);   // A4
    // beat 7: rest

    roll.setInstrument(voice);

    static auto rollConn = masterClock.connect(roll);

    Serial.printf("SeqnautDemo ready — BPM=%.0f  PPQN=%lu\n", masterClock.bpm(), PPQN);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    masterClock.update();
}
