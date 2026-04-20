#pragma once

#include "AudioNode.h"
#include "Notifiable.h"
#include "Filters/svf.h"

#include <array>
#include <cmath>
#include <cstddef>

// ── TransientDetector ─────────────────────────────────────────────────────────
//
// Detects transients on the rising slope of the signal envelope — not at the
// peak.  Fires TransientDetected events as early as possible, well before the
// signal reaches its maximum.
//
// Signal chain (per sample):
//   input → HP filter (attack emphasis) → |x| → envelope follower
//         → derivative → threshold + noise gate + cooldown → event
//
// AudioNode contract:
//   numInputs()  = 1  (mono audio in)
//   numOutputs() = 2
//     ch0 — envelope signal (audio-rate, useful as VCA control input)
//     ch1 — gate signal:
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
        float sampleRate  = 44100.f;
        float hpCutoffHz  = 1200.f;  // attack-emphasis high-pass cutoff
        float attackMs    = 0.5f;    // envelope follower attack time
        float releaseMs   = 10.f;    // envelope follower release time
        float threshold   = 0.01f;   // minimum derivative to fire a trigger
        float noiseFloor  = 0.005f;  // minimum envelope level to qualify
        float cooldownMs  = 30.f;    // refractory period after a trigger
    };

    // ── Event ─────────────────────────────────────────────────────────────────

    struct TransientDetected {
        size_t sampleIndex;    // sample offset within the current block
        float  envelopeLevel;  // envelope value at trigger moment
        float  derivative;     // d = env[n] - env[n-1]; indicates attack sharpness
    };

    // ── Construction ──────────────────────────────────────────────────────────

    explicit TransientDetector(Notifiable<TransientDetected>& listener)
        : TransientDetector(listener, Config{}) {}

    TransientDetector(Notifiable<TransientDetected>& listener, const Config& cfg)
        : _cfg(cfg)
        , _listener(listener)
        , _outputBus(2, AudioBuffer(128, 0.f))
    {
        _hpf.Init(_cfg.sampleRate);
        _hpf.SetFreq(_cfg.hpCutoffHz);
        _hpf.SetRes(0.f);
        _hpf.SetDrive(0.f);

        _attackCoeff     = 1.f - std::exp(-1.f / (_cfg.attackMs  * 0.001f * _cfg.sampleRate));
        _releaseCoeff    = 1.f - std::exp(-1.f / (_cfg.releaseMs * 0.001f * _cfg.sampleRate));
        _cooldownSamples = static_cast<int>(_cfg.cooldownMs * 0.001f * _cfg.sampleRate);

        // Threshold between transient phase (+1) and refractory phase (-1).
        // Counter starts at _cooldownSamples and counts down; values above
        // this threshold are still in the first 10%.
        _transientThreshold = static_cast<int>(_cooldownSamples * 0.9f);
    }

    // ── AudioNode ─────────────────────────────────────────────────────────────

    size_t numInputs()  const override { return 1; }
    size_t numOutputs() const override { return 2; }

    AudioBusView process(AudioBusView inputs) override {
        const AudioBufferView in  = inputs[0];
        const size_t          len = in.size();

        AudioBuffer& envOut  = _outputBus[0];
        AudioBuffer& gateOut = _outputBus[1];
        if (envOut.size()  != len) envOut.resize(len);
        if (gateOut.size() != len) gateOut.resize(len);

        for (size_t n = 0; n < len; ++n) {
            // 1. High-pass filter (emphasise attack frequencies)
            _hpf.Process(in[n]);
            const float hp = _hpf.High();

            // 2. Rectification
            const float rect = std::fabs(hp);

            // 3. Envelope follower — asymmetric IIR
            const float coeff = (rect > _env) ? _attackCoeff : _releaseCoeff;
            _env += coeff * (rect - _env);

            // 4. Derivative — edge detector
            const float d = _env - _envPrev;
            _envPrev = _env;

            // 5. Threshold + noise gate + cooldown
            if (_cooldownCounter > 0) {
                --_cooldownCounter;
            } else if (d > _cfg.threshold && _env > _cfg.noiseFloor) {
                _cooldownCounter = _cooldownSamples;
                _listener.notify({ n, _env, d });
            }

            // 6. Gate output
            //   +1.0 — first 10% of cooldown (transient phase)
            //   -1.0 — remaining 90%         (refractory phase)
            //    0.0 — idle
            if      (_cooldownCounter > _transientThreshold) gateOut[n] =  1.f;
            else if (_cooldownCounter > 0)                   gateOut[n] = -1.f;
            else                                             gateOut[n] =  0.f;

            envOut[n] = _env;
        }

        _outputView[0] = AudioBufferView(envOut);
        _outputView[1] = AudioBufferView(gateOut);
        return AudioBusView(_outputView);
    }

private:
    Config             _cfg;
    daisysp::Svf       _hpf;

    float _env              = 0.f;
    float _envPrev          = 0.f;
    float _attackCoeff      = 0.f;
    float _releaseCoeff     = 0.f;
    int   _cooldownSamples  = 0;
    int   _cooldownCounter  = 0;
    int   _transientThreshold = 0;

    AudioBus                      _outputBus;
    std::array<AudioBufferView, 2> _outputView {};

    Notifiable<TransientDetected>& _listener;
};
