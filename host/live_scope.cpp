#include <csignal>
#include <cstdio>
#include <string>

#include "AudioCardInput.h"
#include "AudioConnection.h"
#include "TerminalScope.h"

static volatile bool running = true;

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
    TerminalScope scope(BLOCK, "input");

    auto c = AudioConnection::from(input).to(scope).in(patch).connect();

    std::printf("Capturing — Ctrl+C to stop\n\n");

    while (running)
        patch.process();

    return 0;
}
