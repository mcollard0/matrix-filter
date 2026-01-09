#include "config.h"
#include "camera_capture.h"
#include "virtual_output.h"
#include "static_effect.h"
#include "matrix_effect.h"

#include <iostream>
#include <chrono>
#include <random>
#include <csignal>
#include <cstring>
#include <getopt.h>

static volatile bool running = true;

void signalHandler(int) {
    running = false;
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Matrix Filter - Virtual camera with Matrix-style glitch effects\n\n"
              << "Options:\n"
              << "  -d, --device <path>        Input camera device (default: auto-detect)\n"
              << "  -o, --output <path>        Virtual camera device (default: /dev/video2)\n"
              << "  --min-interval <minutes>   Minimum interval between effects (default: 1)\n"
              << "  --max-interval <minutes>   Maximum interval between effects (default: 60)\n"
              << "  --effect-duration <ms>     Matrix effect duration (default: 5000)\n"
              << "  --static-frames <count>    Static frames before matrix (default: 10)\n"
              << "  -c, --cycles <count>       Number of effect cycles, 0=infinite (default: 0)\n"
              << "  -t, --test                 Trigger effect immediately on start\n"
              << "  -h, --help                 Show this help\n\n"
              << "Example:\n"
              << "  " << progName << " -d /dev/video0 --min-interval 1 --max-interval 5\n";
}

Config parseArgs(int argc, char* argv[]) {
    Config config;

    static struct option longOptions[] = {
        {"device",          required_argument, nullptr, 'd'},
        {"output",          required_argument, nullptr, 'o'},
        {"min-interval",    required_argument, nullptr, 'm'},
        {"max-interval",    required_argument, nullptr, 'M'},
        {"effect-duration", required_argument, nullptr, 'e'},
        {"static-frames",   required_argument, nullptr, 's'},
        {"cycles",          required_argument, nullptr, 'c'},
        {"test",            no_argument,       nullptr, 't'},
        {"help",            no_argument,       nullptr, 'h'},
        {nullptr,           0,                 nullptr, 0}
    };

    int opt;
    int optionIndex = 0;

    while ((opt = getopt_long(argc, argv, "d:o:c:th", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'd':
                config.inputDevice = optarg;
                break;
            case 'o':
                config.outputDevice = optarg;
                break;
            case 'm':
                config.minInterval = std::stoi(optarg);
                break;
            case 'M':
                config.maxInterval = std::stoi(optarg);
                break;
            case 'e':
                config.effectDuration = std::stoi(optarg);
                break;
            case 's':
                config.staticFrames = std::stoi(optarg);
                break;
            case 'c':
                config.cycles = std::stoi(optarg);
                break;
            case 't':
                config.testMode = true;
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            default:
                printUsage(argv[0]);
                exit(1);
        }
    }

    // Validate
    if (config.minInterval < 1) config.minInterval = 1;
    if (config.maxInterval < config.minInterval) config.maxInterval = config.minInterval;
    if (config.effectDuration < 100) config.effectDuration = 100;
    if (config.staticFrames < 1) config.staticFrames = 1;

    return config;
}

uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

uint64_t randomIntervalMs(int minMinutes, int maxMinutes, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(minMinutes, maxMinutes);
    return static_cast<uint64_t>(dist(rng)) * 60 * 1000;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Config config = parseArgs(argc, argv);

    std::cout << "Matrix Filter starting...\n";
    std::cout << "  Min interval: " << config.minInterval << " minutes\n";
    std::cout << "  Max interval: " << config.maxInterval << " minutes\n";
    std::cout << "  Effect duration: " << config.effectDuration << " ms\n";
    std::cout << "  Static frames: " << config.staticFrames << "\n";

    // Initialize camera
    CameraCapture camera;
    if (config.inputDevice.empty()) {
        std::cout << "Auto-detecting camera...\n";
        if (!camera.detectCamera()) {
            std::cerr << "Failed to detect camera\n";
            return 1;
        }
    } else {
        if (!camera.open(config.inputDevice)) {
            std::cerr << "Failed to open camera: " << config.inputDevice << "\n";
            return 1;
        }
    }

    int width, height;
    camera.getResolution(width, height);
    double fps = camera.getFPS();

    // Initialize virtual output
    VirtualOutput output;
    if (!output.open(config.outputDevice, width, height, fps)) {
        std::cerr << "Failed to open virtual camera\n";
        return 1;
    }

    // Initialize effects
    StaticEffect staticEffect;
    staticEffect.initialize(width, height);

    MatrixEffect matrixEffect;
    if (!matrixEffect.initialize(width, height)) {
        std::cerr << "Warning: Matrix effect initialization had issues\n";
    }

    // Random number generator for timing
    std::mt19937 rng(std::random_device{}());

    // State machine
    EffectState state = EffectState::PASSTHROUGH;
    uint64_t nextEffectTime;
    if (config.testMode) {
        nextEffectTime = getCurrentTimeMs();  // Trigger immediately
        std::cout << "TEST MODE: Effect will trigger immediately\n";
    } else {
        nextEffectTime = getCurrentTimeMs() + randomIntervalMs(config.minInterval, config.maxInterval, rng);
    }
    uint64_t stateStartTime = 0;
    int staticFrameCount = 0;
    int cycleCount = 0;
    bool effectsFinished = false;  // True when all cycles complete

    std::cout << "Running... Press Ctrl+C to stop\n";
    if (config.cycles > 0) {
        std::cout << "Will run " << config.cycles << " effect cycle(s)\n";
    }
    if (!config.testMode) {
        std::cout << "Next effect in " << (nextEffectTime - getCurrentTimeMs()) / 1000 << " seconds\n";
    }

    // Main loop
    while (running) {
        uint64_t currentTime = getCurrentTimeMs();

        // Capture frame from camera
        cv::Mat frame = camera.captureFrame();
        if (frame.empty()) {
            std::cerr << "Failed to capture frame\n";
            continue;
        }

        cv::Mat outputFrame;

        switch (state) {
            case EffectState::PASSTHROUGH:
                if (!effectsFinished && currentTime >= nextEffectTime) {
                    // Trigger effect sequence
                    state = EffectState::STATIC;
                    stateStartTime = currentTime;
                    staticFrameCount = 0;
                    std::cout << "Effect triggered! Showing static...\n";
                }
                outputFrame = frame;
                break;

            case EffectState::STATIC:
                outputFrame = staticEffect.generate();
                staticFrameCount++;

                if (staticFrameCount >= config.staticFrames) {
                    state = EffectState::MATRIX;
                    stateStartTime = currentTime;
                    matrixEffect.reset();
                    std::cout << "Showing matrix effect...\n";
                }
                break;

            case EffectState::MATRIX:
                matrixEffect.update(currentTime);
                outputFrame = matrixEffect.render();

                if (currentTime - stateStartTime >= static_cast<uint64_t>(config.effectDuration)) {
                    state = EffectState::PASSTHROUGH;
                    cycleCount++;

                    // Check if we've completed all cycles
                    if (config.cycles > 0 && cycleCount >= config.cycles) {
                        effectsFinished = true;
                        std::cout << "Completed " << cycleCount << " cycle(s). Passthrough only from now on.\n";
                    } else {
                        nextEffectTime = currentTime + randomIntervalMs(config.minInterval, config.maxInterval, rng);
                        std::cout << "Returning to passthrough. Next effect in "
                                  << (nextEffectTime - currentTime) / 1000 << " seconds\n";
                    }
                }
                break;
        }

        // Write to virtual camera
        output.writeFrame(outputFrame);

        // Small delay to maintain frame rate
        int frameDelayMs = static_cast<int>(1000.0 / fps);
        cv::waitKey(frameDelayMs);
    }

    std::cout << "\nShutting down...\n";
    camera.close();
    output.close();

    return 0;
}
