#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "AudioConnection.h"
#include "TransientDetector.h"
#include "WavReader.h"
#include "WavWriter.h"

#include <cstdio>
#include <string>

// ── TimestampLogger ───────────────────────────────────────────────────────────

class TimestampLogger : public Notifiable<TransientDetector::TransientDetected> {
public:
    explicit TimestampLogger(float sampleRate) : _sampleRate(sampleRate) {}

    void notify(const TransientDetector::TransientDetected& e) override {
        const double ms = (_blockOffset + static_cast<double>(e.sampleIndex))
                          / _sampleRate * 1000.0;
        std::printf("TRIGGER  %8.2f ms  env=%.4f  d=%.4f\n",
                    ms, e.envelopeLevel, e.derivative);
    }

    void setBlockOffset(size_t offset) { _blockOffset = offset; }

private:
    float  _sampleRate;
    size_t _blockOffset = 0;
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: transient_detector <file.wav> [hpCutoffHz] [threshold] [cooldownMs]\n");
        return 1;
    }

    constexpr size_t BLOCK = 128;

    WavReader source(argv[1], BLOCK);

    TransientDetector::Config cfg;
    cfg.sampleRate = source.sampleRate();
    if (argc >= 3) cfg.hpCutoffHz = std::stof(argv[2]);
    if (argc >= 4) cfg.threshold  = std::stof(argv[3]);
    if (argc >= 5) cfg.cooldownMs = std::stof(argv[4]);

    std::printf("File      : %s\n", argv[1]);
    std::printf("SampleRate: %.0f Hz\n", cfg.sampleRate);
    std::printf("Config    : hp=%.0f Hz  attack=%.2f ms  release=%.2f ms"
                "  threshold=%.4f  floor=%.4f  cooldown=%.0f ms\n\n",
                cfg.hpCutoffHz, cfg.attackMs, cfg.releaseMs,
                cfg.threshold, cfg.noiseFloor, cfg.cooldownMs);

    // ── Build graph ───────────────────────────────────────────────────────────

    Patch patch(BLOCK, cfg.sampleRate);

    TimestampLogger   logger(cfg.sampleRate);
    TransientDetector detector(logger, cfg);
    WavWriter         envWriter("envelope.wav", cfg.sampleRate);
    WavWriter         gateWriter("gate.wav",     cfg.sampleRate);

    auto c1 = AudioConnection::from(source)
                              .to(detector)
                              .in(patch)
                              .connect();

    auto c2 = AudioConnection::from(detector).output(0)
                              .to(envWriter) .input(0)
                              .in(patch)
                              .connect();

    auto c3 = AudioConnection::from(detector).output(1)
                              .to(gateWriter).input(0)
                              .in(patch)
                              .connect();

    // ── Run ───────────────────────────────────────────────────────────────────

    size_t blockOffset = 0;
    while (!source.done()) {
        logger.setBlockOffset(blockOffset);
        patch.process();
        blockOffset += BLOCK;
    }

    return 0;
}
