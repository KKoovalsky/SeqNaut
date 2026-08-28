// TransientDetectorDemo — blinks LED on every detected transient
// Teensy 4.1 + PJRC Audio Shield (SGTL5000)

#include <Audio.h>
#include <SPI.h>
#include <Wire.h>

#include "AudioNodeStream.h"
#include "Notifiable.h"
#include "TransientDetector.h"

static constexpr int LED_PIN = 13;
static constexpr unsigned long LED_ON_MS = 50;

// Scope-visible trigger pulse (CH2): as narrow as the scope's sample rate can
// reliably catch, so back-to-back retriggers a few ms apart show as distinct
// pulses instead of merging into one continuous high region like the LED does.
static constexpr int TRIGGER_PULSE_PIN = 2;
static constexpr unsigned int TRIGGER_PULSE_US = 10;

// ── ISR → loop() bridge ───────────────────────────────────────────────────────
struct TransientListener : Notifiable<TransientDetector::TransientDetected> {
    volatile bool fired = false;
    void notify(const TransientDetector::TransientDetected&) override { fired = true; }
};

// ── Audio graph ───────────────────────────────────────────────────────────────
TransientListener listener;
TransientDetector detector(listener);
AudioNodeStream   detectorStream(detector);
AudioInputI2S     input;
AudioControlSGTL5000 codec;
AudioAnalyzePeak  peakMeter;

AudioConnection inputToDetector(input, 0, detectorStream, 0);
AudioConnection inputToPeakMeter(input, 0, peakMeter, 0);

// ── LED state ─────────────────────────────────────────────────────────────────
static elapsedMillis ledTimer;
static bool ledOn = false;

// ── Heartbeat ─────────────────────────────────────────────────────────────────
static elapsedMillis heartbeatTimer;
static unsigned long heartbeatSeconds = 0;

// ── Peak meter (source-level calibration: match guitar vs. PC playback) ───────
static elapsedMillis peakPrintTimer;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(TRIGGER_PULSE_PIN, OUTPUT);
    digitalWriteFast(TRIGGER_PULSE_PIN, LOW);
    Serial.begin(921600);
    AudioMemory(16);
    codec.enable();
    codec.inputSelect(AUDIO_INPUT_LINEIN);
    codec.lineInLevel(5);
    Serial.println("TransientDetectorDemo ready");
}

void loop() {
    if (listener.fired) {
        listener.fired = false;
        digitalWrite(LED_PIN, HIGH);
        ledTimer = 0;
        ledOn = true;
        Serial.print("TRANSIENT ");
        Serial.println(millis());

        digitalWriteFast(TRIGGER_PULSE_PIN, HIGH);
        delayMicroseconds(TRIGGER_PULSE_US);
        digitalWriteFast(TRIGGER_PULSE_PIN, LOW);
    }
    if (ledOn && ledTimer >= LED_ON_MS) {
        digitalWrite(LED_PIN, LOW);
        ledOn = false;
    }
    if (heartbeatTimer >= 1000) {
        heartbeatTimer = 0;
        Serial.print("heartbeat ");
        Serial.println(++heartbeatSeconds);
    }
    if (peakPrintTimer >= 200) {
        peakPrintTimer = 0;
        if (peakMeter.available()) {
            Serial.print("peak ");
            Serial.println(peakMeter.read(), 4);
        }
    }
}
