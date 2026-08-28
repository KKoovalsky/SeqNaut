#define DR_WAV_IMPLEMENTATION
#include <cstdio>
#include <filesystem>
#include <string>

#include "AudioConnection.h"
#include "TransientDetector.h"
#include "WavReader.h"
#include "WavWriter.h"
#include "dr_wav.h"

// ── TimestampLogger ───────────────────────────────────────────────────────────

class TimestampLogger : public Notifiable<TransientDetector::TransientDetected> {
public:
    explicit TimestampLogger(float sampleRate) : _sampleRate(sampleRate) {}

    void notify(const TransientDetector::TransientDetected& e) override {
        const double ms = (_blockOffset + static_cast<double>(e.sampleIndex)) / _sampleRate * 1000.0;
        std::printf("TRIGGER  %8.2f ms  fast=%.4f  attack=%.4f\n", ms, e.fastEnv, e.attackMetric);
    }

    void setBlockOffset(size_t offset) {
        _blockOffset = offset;
    }

private:
    float _sampleRate;
    size_t _blockOffset = 0;
};

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: transient_detector <file.wav> [hpCutoffHz] [threshold] [cooldownMs] [reArmMs] [confirmMs]\n");
        return 1;
    }

    constexpr size_t BLOCK = 128;

    const std::string stem = std::filesystem::path(argv[1]).stem().string();
    WavReader source(argv[1], BLOCK);

    TransientDetector::Config cfg;
    cfg.sampleRate = source.sampleRate();
    if (argc >= 3)
        cfg.hpCutoffHz = std::stof(argv[2]);
    if (argc >= 4)
        cfg.thresholdK = std::stof(argv[3]);
    if (argc >= 5)
        cfg.cooldownMs = std::stof(argv[4]);
    if (argc >= 6)
        cfg.reArmMs = std::stof(argv[5]);
    if (argc >= 7)
        cfg.confirmMs = std::stof(argv[6]);
    if (argc >= 8)
        cfg.noiseFloor = std::stof(argv[7]);

    std::printf("File      : %s\n", argv[1]);
    std::printf("SampleRate: %.0f Hz\n", cfg.sampleRate);
    std::printf(
        "Config    : hp=%.0f Hz"
        "  fast=%.1f/%.1f ms  slow=%.0f ms  bg=%.0f ms"
        "  k=%.2f  offset=%.4f  floor=%.4f  cooldown=%.0f ms  reArm=%.0f ms  confirm=%.0f ms\n\n",
        cfg.hpCutoffHz, cfg.fastAttackMs, cfg.fastReleaseMs, cfg.slowMs, cfg.bgMs, cfg.thresholdK, cfg.thresholdOffset,
        cfg.noiseFloor, cfg.cooldownMs, cfg.reArmMs, cfg.confirmMs);

    // ── Build graph ───────────────────────────────────────────────────────────

    Patch patch(BLOCK, cfg.sampleRate);

    TimestampLogger logger(cfg.sampleRate);
    TransientDetector detector(logger, cfg);
    WavWriter fastEnvWriter(stem + "_fast_env.wav", cfg.sampleRate);
    WavWriter slowEnvWriter(stem + "_slow_env.wav", cfg.sampleRate);
    WavWriter gateWriter(stem + "_gate.wav", cfg.sampleRate);

    auto c1 = AudioConnection::from(source).to(detector).in(patch).connect();
    auto c2 = AudioConnection::from(detector).output(0).to(fastEnvWriter).input(0).in(patch).connect();
    auto c3 = AudioConnection::from(detector).output(1).to(slowEnvWriter).input(0).in(patch).connect();
    auto c4 = AudioConnection::from(detector).output(2).to(gateWriter).input(0).in(patch).connect();

    // ── Run ───────────────────────────────────────────────────────────────────

    size_t blockOffset = 0;
    while (!source.done()) {
        logger.setBlockOffset(blockOffset);
        patch.process();
        blockOffset += BLOCK;
    }

    return 0;
}
