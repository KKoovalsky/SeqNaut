#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "AudioConnection.h"
#include "BufferSource.h"
#include "TransientDetector.h"
#include "WavWriter.h"

#include <cstdio>
#include <string>
#include <vector>

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

// ── WAV loading ───────────────────────────────────────────────────────────────

static std::vector<float> loadMono(const char* path, unsigned int& outSampleRate) {
    drwav wav;
    if (!drwav_init_file(&wav, path, nullptr)) {
        std::fprintf(stderr, "error: cannot open '%s'\n", path);
        return {};
    }

    const size_t totalFrames = static_cast<size_t>(wav.totalPCMFrameCount);
    outSampleRate = wav.sampleRate;

    std::vector<float> interleaved(totalFrames * wav.channels);
    drwav_read_pcm_frames_f32(&wav, totalFrames, interleaved.data());
    drwav_uninit(&wav);

    if (wav.channels == 1)
        return interleaved;

    const float scale = 1.f / static_cast<float>(wav.channels);
    std::vector<float> mono(totalFrames, 0.f);
    for (size_t i = 0; i < totalFrames; ++i)
        for (unsigned ch = 0; ch < wav.channels; ++ch)
            mono[i] += interleaved[i * wav.channels + ch] * scale;

    return mono;
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: transient_detector <file.wav> [hpCutoffHz] [threshold] [cooldownMs]\n");
        return 1;
    }

    unsigned int sampleRate = 44100;
    const std::vector<float> samples = loadMono(argv[1], sampleRate);
    if (samples.empty()) return 1;

    constexpr size_t BLOCK = 128;

    TransientDetector::Config cfg;
    cfg.sampleRate = static_cast<float>(sampleRate);
    if (argc >= 3) cfg.hpCutoffHz = std::stof(argv[2]);
    if (argc >= 4) cfg.threshold  = std::stof(argv[3]);
    if (argc >= 5) cfg.cooldownMs = std::stof(argv[4]);

    std::printf("File      : %s\n", argv[1]);
    std::printf("SampleRate: %u Hz\n", sampleRate);
    std::printf("Samples   : %zu  (%.2f s)\n", samples.size(),
                static_cast<double>(samples.size()) / sampleRate);
    std::printf("Config    : hp=%.0f Hz  attack=%.2f ms  release=%.2f ms"
                "  threshold=%.4f  floor=%.4f  cooldown=%.0f ms\n\n",
                cfg.hpCutoffHz, cfg.attackMs, cfg.releaseMs,
                cfg.threshold, cfg.noiseFloor, cfg.cooldownMs);

    // ── Build graph ───────────────────────────────────────────────────────────

    Patch patch(BLOCK, cfg.sampleRate);

    TimestampLogger   logger(cfg.sampleRate);
    BufferSource      source(samples, BLOCK);
    TransientDetector detector(logger, cfg);
    WavWriter         envWriter("envelope.wav", cfg.sampleRate);

    auto c1 = AudioConnection::from(source)
                              .to(detector)
                              .in(patch)
                              .connect();

    auto c2 = AudioConnection::from(detector).output(0)
                              .to(envWriter) .input(0)
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
