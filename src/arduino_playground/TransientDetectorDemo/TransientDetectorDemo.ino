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

AudioConnection inputToDetector(input, 0, detectorStream, 0);

// ── LED state ─────────────────────────────────────────────────────────────────
static elapsedMillis ledTimer;
static bool ledOn = false;

// ── Heartbeat ─────────────────────────────────────────────────────────────────
static elapsedMillis heartbeatTimer;
static unsigned long heartbeatSeconds = 0;

void setup() {
    pinMode(LED_PIN, OUTPUT);
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
}
