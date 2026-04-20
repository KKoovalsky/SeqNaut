#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "AudioNode.h"
#include "Filters/svf.h"
#include "Notifiable.h"

// ── TransientDetector ─────────────────────────────────────────────────────────
//
// Detects guitar transients on the rising edge of the attack — before the peak.
//
// Signal chain (per sample):
//   input
//     → HPF (emphasise pick attack)
//     → |x|
//     → fast envelope
//     → slow envelope (background)
//     → attack metric = fast - slow
//     → adaptive threshold = k * attack_bg + offset
//     → floor check (fast > noiseFloor)
//     → cooldown
//     → trigger
//
// The adaptive threshold tracks the recent background of the attack metric,
// making the detector sensitive to rapid energy rises relative to local context
// rather than fixed absolute levels.
//
// AudioNode contract:
//   numInputs()  = 1  (mono audio in)
//   numOutputs() = 3
//     ch0 — fast envelope
//     ch1 — slow envelope
//     ch2 — gate signal:
//              +1.0  first 10% of cooldown  (transient phase)
//              -1.0  remaining 90%          (refractory phase)
//               0.0  idle
//
// All listener callbacks fire synchronously inside process().  They must be
// real-time safe: no allocation, no blocking I/O.

class TransientDetector : public AudioNode {
public:
    // ── Config ────────────────────────────────────────────────────────────────

    struct Config {
        float sampleRate = 44100.f;
        float hpCutoffHz = 1200.f;       // detection-path high-pass cutoff
        float fastAttackMs = 0.2f;       // fast envelope attack time
        float fastReleaseMs = 30.f;      // fast envelope release time
        float slowMs = 50.f;             // slow background envelope time constant
        float bgMs = 200.f;              // attack metric background tracker time constant
        float thresholdK = 1.5f;         // adaptive threshold multiplier
        float thresholdOffset = 0.002f;  // minimum threshold margin (prevents collapse in silence)
        float noiseFloor = 0.005f;       // minimum fast envelope level to qualify
        float cooldownMs = 30.f;         // refractory period after a trigger
    };

    // ── Event ─────────────────────────────────────────────────────────────────

    struct TransientDetected {
        size_t sampleIndex;  ///< Sample offset within the current block.
        float fastEnv;       ///< Fast envelope value at trigger moment.
        float attackMetric;  ///< fast - slow at trigger moment; indicates attack sharpness.
    };

    // ── Construction ──────────────────────────────────────────────────────────

    explicit TransientDetector(Notifiable<TransientDetected>& listener) : TransientDetector(listener, Config{}) {}

    TransientDetector(Notifiable<TransientDetected>& listener, const Config& cfg)
        : _cfg(cfg),
          _listener(listener),
          _outputBus(3, AudioBuffer(128, 0.f)) {
        _hpf.Init(_cfg.sampleRate);
        _hpf.SetFreq(_cfg.hpCutoffHz);
        _hpf.SetRes(0.f);
        _hpf.SetDrive(0.f);

        _fastAttackCoeff = coeff(_cfg.fastAttackMs);
        _fastReleaseCoeff = coeff(_cfg.fastReleaseMs);
        _slowCoeff = coeff(_cfg.slowMs);
        _bgCoeff = coeff(_cfg.bgMs);
        _cooldownSamples = static_cast<int>(_cfg.cooldownMs * 0.001f * _cfg.sampleRate);
        _transientThreshold = static_cast<int>(_cooldownSamples * 0.9f);
    }

    // ── AudioNode ─────────────────────────────────────────────────────────────

    size_t numInputs() const override {
        return 1;
    }
    size_t numOutputs() const override {
        return 3;
    }

    AudioBusView process(AudioBusView inputs) override {
        const AudioBufferView in = inputs[0];
        const size_t len = in.size();

        AudioBuffer& fastOut = _outputBus[0];
        AudioBuffer& slowOut = _outputBus[1];
        AudioBuffer& gateOut = _outputBus[2];
        if (fastOut.size() != len)
            fastOut.resize(len);
        if (slowOut.size() != len)
            slowOut.resize(len);
        if (gateOut.size() != len)
            gateOut.resize(len);

        for (size_t n = 0; n < len; ++n) {
            // 1. High-pass filter — emphasise pick attack, suppress low-freq body
            _hpf.Process(in[n]);
            const float hp = _hpf.High();

            // 2. Rectification
            const float rect = std::fabs(hp);

            // 3. Fast envelope — tracks short-term energy rise immediately
            const float fastCoeff = (rect > _fastEnv) ? _fastAttackCoeff : _fastReleaseCoeff;
            _fastEnv += fastCoeff * (rect - _fastEnv);

            // 4. Slow envelope — tracks local background level
            _slowEnv += _slowCoeff * (rect - _slowEnv);

            // 5. Attack metric — how much fast energy exceeds local background
            const float attack = std::max(0.f, _fastEnv - _slowEnv);

            // 6. Adaptive threshold background — slow tracker of the attack metric itself
            _attackBg += _bgCoeff * (attack - _attackBg);
            const float threshold = _cfg.thresholdK * _attackBg + _cfg.thresholdOffset;

            // 7. Trigger detection
            if (_cooldownCounter > 0) {
                --_cooldownCounter;
            } else if (attack > threshold && _fastEnv > _cfg.noiseFloor) {
                _cooldownCounter = _cooldownSamples;
                _listener.notify({n, _fastEnv, attack});
            }

            // 8. Gate output
            //   +1.0 — first 10% of cooldown (transient phase)
            //   -1.0 — remaining 90%         (refractory phase)
            //    0.0 — idle
            if (_cooldownCounter > _transientThreshold)
                gateOut[n] = 1.f;
            else if (_cooldownCounter > 0)
                gateOut[n] = -1.f;
            else
                gateOut[n] = 0.f;

            fastOut[n] = _fastEnv;
            slowOut[n] = _slowEnv;
        }

        _outputView[0] = AudioBufferView(fastOut);
        _outputView[1] = AudioBufferView(slowOut);
        _outputView[2] = AudioBufferView(gateOut);
        return AudioBusView(_outputView);
    }

private:
    float coeff(float ms) const {
        return 1.f - std::exp(-1.f / (ms * 0.001f * _cfg.sampleRate));
    }

    Config _cfg;
    daisysp::Svf _hpf;

    float _fastEnv = 0.f;
    float _slowEnv = 0.f;
    float _attackBg = 0.f;

    float _fastAttackCoeff = 0.f;
    float _fastReleaseCoeff = 0.f;
    float _slowCoeff = 0.f;
    float _bgCoeff = 0.f;

    int _cooldownSamples = 0;
    int _cooldownCounter = 0;
    int _transientThreshold = 0;

    AudioBus _outputBus;
    std::array<AudioBufferView, 3> _outputView{};

    Notifiable<TransientDetected>& _listener;
};
