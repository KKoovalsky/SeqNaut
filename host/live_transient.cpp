#define DR_WAV_IMPLEMENTATION
#include <atomic>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <string>

#include "AudioCardInput.h"
#include "AudioConnection.h"
#include "Notifiable.h"
#include "TerminalScope.h"
#include "TransientDetector.h"
#include "WavWriter.h"

static volatile bool running = true;

struct TransientCounter : Notifiable<TransientDetector::TransientDetected> {
    std::atomic<int> count{0};
    void notify(const TransientDetector::TransientDetected&) override {
        count.fetch_add(1, std::memory_order_relaxed);
    }
};

int main(int argc, char** argv) {
    constexpr size_t BLOCK = 128;
    constexpr float SAMPLE_RATE = 48000.f;

    if (argc == 2 && std::string(argv[1]) == "--list") {
        AudioCardInput::listDevices();
        return 0;
    }

    std::signal(SIGINT, [](int) { running = false; });

    Patch patch(BLOCK, SAMPLE_RATE);

    AudioCardInput input = argc >= 2
        ? AudioCardInput(BLOCK, SAMPLE_RATE, std::string_view(argv[1]))
        : AudioCardInput(BLOCK, SAMPLE_RATE);

    TransientCounter counter;
    TransientDetector::Config cfg{
        .sampleRate = SAMPLE_RATE,
        .thresholdOffset = 0.001f,
        .noiseFloor = 0.001f,
    };
    TransientDetector detector(counter, cfg);

    TerminalScope fastScope(BLOCK, "fast_env");
    TerminalScope slowScope(BLOCK, "slow_env");
    TerminalScope gateScope(BLOCK, "gate");

    const std::time_t now = std::time(nullptr);
    char ts[20];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));

    WavWriter inputWriter(std::string("live_input_") + ts + ".wav", SAMPLE_RATE);
    WavWriter gateWriter(std::string("live_gate_") + ts + ".wav", SAMPLE_RATE);

    auto c0 = AudioConnection::from(input).to(detector).in(patch).connect();
    auto c1 = AudioConnection::from(detector).output(0).to(fastScope).in(patch).connect();
    auto c2 = AudioConnection::from(detector).output(1).to(slowScope).in(patch).connect();
    auto c3 = AudioConnection::from(detector).output(2).to(gateScope).in(patch).connect();
    auto c4 = AudioConnection::from(input).to(inputWriter).in(patch).connect();
    auto c5 = AudioConnection::from(detector).output(2).to(gateWriter).in(patch).connect();

    std::printf("Recording live_input_%s.wav + live_gate_%s.wav — Ctrl+C to stop\n\n", ts, ts);

    int lastCount = 0;
    while (running) {
        patch.process();
        const int now = counter.count.load(std::memory_order_relaxed);
        if (now != lastCount) {
            std::printf("*** TRANSIENT #%d\n", now);
            lastCount = now;
        }
    }

    return 0;
}
