#include "config.h"
#include "camera_capture.h"
#include "virtual_output.h"
#include "static_effect.h"
#include "matrix_effect.h"
#include "time_utils.h"
#include "consumer_detector.h"

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
              << "Time values accept units: ms, s, m, h (e.g., 500ms, 5s, 2m, 1h)\n\n"
              << "Options:\n"
              << "  -d, --device <path>        Input camera device (default: auto-detect)\n"
              << "  -o, --output <path>        Virtual camera device (default: /dev/video2)\n"
              << "  -r, --res <level>          Resolution: high, medium, low (default: high)\n"
              << "  --min-interval <time>      Minimum interval between effects (default: 1m)\n"
              << "  --max-interval <time>      Maximum interval between effects (default: 60m)\n"
              << "  --effect-duration <time>   Matrix effect duration (default: 5s)\n"
              << "  --static-duration <time>   Static effect duration (default: 300ms)\n"
              << "  --start-delay <time>       Initial delay before first effect (default: random)\n"
              << "  -c, --cycles <count>       Number of effect cycles, 0=infinite (default: 0)\n"
              << "  -t, --test                 Trigger effect immediately (same as --start-delay 0)\n"
              << "  --no-on-demand             Keep camera open always (don't wait for consumers)\n"
              << "  --overlay                  Overlay matrix effect on camera feed (90% opacity)\n"
              << "  -h, --help                 Show this help\n\n"
              << "On-demand mode (default):\n"
              << "  The physical camera is only opened when an application connects to the\n"
              << "  virtual camera. This allows other apps to use the camera when the virtual\n"
              << "  camera isn't in use. Static frames are shown while the camera initializes.\n\n"
              << "Examples:\n"
              << "  " << progName << " --test --effect-duration 500ms --static-duration 300ms\n"
              << "  " << progName << " --min-interval 5m --max-interval 30m\n"
              << "  " << progName << " --start-delay 10s --effect-duration 3s\n"
              << "  " << progName << " --no-on-demand  # Always keep camera open\n";
}

Config parseArgs(int argc, char* argv[]) {
    Config config;

    static struct option longOptions[] = {
        {"device",          required_argument, nullptr, 'd'},
        {"output",          required_argument, nullptr, 'o'},
        {"res",             required_argument, nullptr, 'r'},
        {"min-interval",    required_argument, nullptr, 'm'},
        {"max-interval",    required_argument, nullptr, 'M'},
        {"effect-duration", required_argument, nullptr, 'e'},
        {"static-duration", required_argument, nullptr, 's'},
        {"start-delay",     required_argument, nullptr, 'D'},
        {"cycles",          required_argument, nullptr, 'c'},
        {"test",            no_argument,       nullptr, 't'},
        {"no-on-demand",    no_argument,       nullptr, 'O'},
        {"overlay",         no_argument,       nullptr, 'Y'},
        {"help",            no_argument,       nullptr, 'h'},
        {nullptr,           0,                 nullptr, 0}
    };

    int opt;
    int optionIndex = 0;
    bool startDelaySet = false;

    while ((opt = getopt_long(argc, argv, "d:o:r:c:th", longOptions, &optionIndex)) != -1) {
        try {
            switch (opt) {
                case 'd':
                    config.inputDevice = optarg;
                    break;
                case 'o':
                    config.outputDevice = optarg;
                    break;
                case 'r': {
                    std::string res = optarg;
                    if (res == "high" || res == "HIGH") {
                        config.resolution = Resolution::HIGH;
                    } else if (res == "medium" || res == "MEDIUM" || res == "med") {
                        config.resolution = Resolution::MEDIUM;
                    } else if (res == "low" || res == "LOW") {
                        config.resolution = Resolution::LOW;
                    } else {
                        std::cerr << "Invalid resolution: " << res << " (use high, medium, or low)\n";
                        exit(1);
                    }
                    break;
                }
                case 'm':
                    config.minInterval = parseTime(optarg);
                    break;
                case 'M':
                    config.maxInterval = parseTime(optarg);
                    break;
                case 'e':
                    config.effectDuration = parseTime(optarg);
                    break;
                case 's':
                    config.staticDuration = parseTime(optarg);
                    break;
                case 'D':
                    config.startDelay = parseTime(optarg);
                    startDelaySet = true;
                    break;
                case 'c':
                    config.cycles = std::stoi(optarg);
                    break;
                case 't':
                    config.testMode = true;
                    config.startDelay = 0;
                    startDelaySet = true;
                    break;
                case 'O':
                    config.onDemand = false;
                    break;
                case 'Y':
                    config.overlay = true;
                    break;
                case 'h':
                    printUsage(argv[0]);
                    exit(0);
                default:
                    printUsage(argv[0]);
                    exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing argument: " << e.what() << "\n";
            exit(1);
        }
    }

    // Validate
    if (config.minInterval < 1) config.minInterval = 1;
    if (config.maxInterval < config.minInterval) config.maxInterval = config.minInterval;
    if (config.effectDuration < 10) config.effectDuration = 10;
    if (config.staticDuration < 10) config.staticDuration = 10;

    // If start delay wasn't explicitly set, mark it for random interval
    if (!startDelaySet) {
        config.startDelay = UINT64_MAX;  // Sentinel for "use random"
    }

    return config;
}

uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

uint64_t randomIntervalMs(uint64_t minMs, uint64_t maxMs, std::mt19937& rng) {
    std::uniform_int_distribution<uint64_t> dist(minMs, maxMs);
    return dist(rng);
}

// Try to detect and open a camera, returns true on success
bool tryOpenCamera(CameraCapture& camera, const Config& config, int& width, int& height) {
    bool success = false;

    if (config.inputDevice.empty()) {
        success = camera.detectCamera(config.resolution);
    } else {
        success = camera.open(config.inputDevice, config.resolution);
    }

    if (success) {
        camera.getResolution(width, height);
    }

    return success;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Config config = parseArgs(argc, argv);

    std::cout << "Matrix Filter starting...\n";
    std::cout << "  Resolution preference: "
              << (config.resolution == Resolution::HIGH ? "high" :
                  config.resolution == Resolution::MEDIUM ? "medium" : "low") << "\n";
    std::cout << "  Min interval: " << formatTime(config.minInterval) << "\n";
    std::cout << "  Max interval: " << formatTime(config.maxInterval) << "\n";
    std::cout << "  Effect duration: " << formatTime(config.effectDuration) << "\n";
    std::cout << "  Static duration: " << formatTime(config.staticDuration) << "\n";
    std::cout << "  On-demand mode: " << (config.onDemand ? "enabled" : "disabled") << "\n";
    std::cout << "  Overlay mode: " << (config.overlay ? "enabled" : "disabled") << "\n";

    // Default resolution for virtual camera based on config preference
    // This prevents blurry output when consumer connects before camera is probed
    int width, height;
    switch (config.resolution) {
        case Resolution::HIGH:
            width = 1920; height = 1080;
            break;
        case Resolution::MEDIUM:
            width = 1280; height = 720;
            break;
        case Resolution::LOW:
        default:
            width = 640; height = 480;
            break;
    }
    double fps = 30.0;

    CameraCapture camera;

    // If not on-demand, open camera immediately
    if (!config.onDemand) {
        std::cout << "Opening camera (on-demand disabled)...\n";
        if (config.inputDevice.empty()) {
            std::cout << "Auto-detecting camera...\n";
            if (!camera.detectCamera(config.resolution)) {
                std::cerr << "Failed to detect camera\n";
                return 1;
            }
        } else {
            if (!camera.open(config.inputDevice, config.resolution)) {
                std::cerr << "Failed to open camera: " << config.inputDevice << "\n";
                return 1;
            }
        }
        camera.getResolution(width, height);
        fps = camera.getFPS();
    } else {
        // On-demand mode: probe camera briefly to get resolution, then close
        std::cout << "Probing camera for resolution...\n";
        if (tryOpenCamera(camera, config, width, height)) {
            fps = camera.getFPS();
            camera.close();
            std::cout << "Camera probed: " << width << "x" << height << " @ " << fps << " FPS\n";
        } else {
            std::cout << "Camera not available, using default " << width << "x" << height << "\n";
            std::cout << "Will probe again when consumer connects.\n";
        }
    }

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

    // Consumer detector for on-demand mode
    ConsumerDetector consumerDetector(config.outputDevice);

    // Random number generator for timing
    std::mt19937 rng(std::random_device{}());

    // Camera state (for on-demand mode)
    CameraState cameraState = config.onDemand ? CameraState::IDLE : CameraState::ACTIVE;
    uint64_t lastCameraPollTime = 0;
    bool hadConsumers = false;

    // Effect state machine
    EffectState effectState = EffectState::PASSTHROUGH;
    uint64_t nextEffectTime = 0;
    uint64_t effectStartTime = 0;
    bool effectTimerInitialized = false;

    uint64_t stateStartTime = 0;
    int cycleCount = 0;
    bool effectsFinished = false;

    std::cout << "Running... Press Ctrl+C to stop\n";
    if (config.cycles > 0) {
        std::cout << "Will run " << config.cycles << " effect cycle(s)\n";
    }
    if (config.onDemand) {
        std::cout << "Waiting for consumer to connect to " << config.outputDevice << "...\n";
    }

    // Main loop
    while (running) {
        uint64_t currentTime = getCurrentTimeMs();
        cv::Mat outputFrame;
        bool hasConsumers = true;

        // On-demand mode: check for consumers
        if (config.onDemand) {
            hasConsumers = consumerDetector.hasConsumers();

            // Handle consumer connect/disconnect
            if (hasConsumers && !hadConsumers) {
                std::cout << "Consumer connected!\n";
                cameraState = CameraState::CONNECTING;
            } else if (!hasConsumers && hadConsumers) {
                std::cout << "Consumer disconnected.\n";
                if (camera.isOpened()) {
                    camera.close();
                    std::cout << "Camera released.\n";
                }
                cameraState = CameraState::IDLE;
                // Reset effect timer for next connection
                effectTimerInitialized = false;
                effectState = EffectState::PASSTHROUGH;
                staticEffect.resetForIdle();  // Start growing animation
            }
            hadConsumers = hasConsumers;
        }

        // Handle camera states
        switch (cameraState) {
            case CameraState::IDLE:
                // No consumers, output static slowly (low CPU)
                outputFrame = staticEffect.generate();
                break;  // Still need to write frame for v4l2loopback

            case CameraState::CONNECTING: {
                // Save current resolution before trying to open camera
                int prevW = width;
                int prevH = height;

                // Try to open camera
                if (tryOpenCamera(camera, config, width, height)) {
                    fps = camera.getFPS();
                    std::cout << "Camera opened: " << width << "x" << height << " @ " << fps << " FPS\n";
                    cameraState = CameraState::ACTIVE;

                    // Check if resolution changed from what virtual output was configured for
                    int outputW = output.getWidth();
                    int outputH = output.getHeight();

                    if (width != outputW || height != outputH) {
                        // Resolution mismatch - consumer is already connected, can't reconfigure
                        // v4l2loopback doesn't allow format change while consumer is reading
                        // We'll scale frames instead (already done in writeFrame)
                        std::cout << "Note: Camera resolution (" << width << "x" << height
                                  << ") differs from virtual output (" << outputW << "x" << outputH
                                  << "). Scaling frames.\n";
                        // Keep effects at camera resolution for quality
                        staticEffect.initialize(width, height);
                        matrixEffect.initialize(width, height);
                    }
                } else {
                    std::cout << "Camera unavailable, polling...\n";
                    cameraState = CameraState::UNAVAILABLE;
                    lastCameraPollTime = currentTime;
                    staticEffect.resetForIdle();  // Start growing animation
                }
                outputFrame = staticEffect.generate();
                break;
            }

            case CameraState::UNAVAILABLE:
                // Camera busy, poll periodically
                if (currentTime - lastCameraPollTime >= config.cameraPollInterval) {
                    if (tryOpenCamera(camera, config, width, height)) {
                        fps = camera.getFPS();
                        std::cout << "Camera now available: " << width << "x" << height << "\n";
                        cameraState = CameraState::ACTIVE;

                        // Reinitialize effects for camera resolution
                        staticEffect.initialize(width, height);
                        matrixEffect.initialize(width, height);

                        // Check if output resolution differs (consumer-locked)
                        int outputW = output.getWidth();
                        int outputH = output.getHeight();
                        if (width != outputW || height != outputH) {
                            std::cout << "Note: Scaling camera (" << width << "x" << height
                                      << ") to output (" << outputW << "x" << outputH << ")\n";
                            // Don't try to reopen output - consumer has it locked
                        }
                    }
                    lastCameraPollTime = currentTime;
                }
                outputFrame = staticEffect.generate();
                break;

            case CameraState::ACTIVE:
                // Initialize effect timer on first active frame
                if (!effectTimerInitialized) {
                    if (config.testMode) {
                        nextEffectTime = currentTime;
                        std::cout << "TEST MODE: Effect will trigger immediately\n";
                    } else if (config.startDelay == UINT64_MAX) {
                        nextEffectTime = currentTime + randomIntervalMs(config.minInterval, config.maxInterval, rng);
                        std::cout << "Next effect in " << formatTime(nextEffectTime - currentTime) << "\n";
                    } else {
                        nextEffectTime = currentTime + config.startDelay;
                        if (config.startDelay > 0) {
                            std::cout << "Start delay: " << formatTime(config.startDelay) << "\n";
                        }
                    }
                    effectTimerInitialized = true;
                }

                // Capture frame from camera
                {
                    cv::Mat frame = camera.captureFrame();
                    if (frame.empty()) {
                        // Camera might have been disconnected
                        std::cerr << "Failed to capture frame, camera may be unavailable\n";
                        camera.close();
                        cameraState = CameraState::UNAVAILABLE;
                        lastCameraPollTime = currentTime;
                        outputFrame = staticEffect.generate();
                        break;
                    }

                    // Effect state machine
                    switch (effectState) {
                        case EffectState::PASSTHROUGH:
                            if (!effectsFinished && currentTime >= nextEffectTime) {
                                effectState = EffectState::STATIC;
                                stateStartTime = currentTime;
                                staticEffect.resetForEffect();  // Full-size static for effect
                                std::cout << "Effect triggered! Showing static...\n";
                            }
                            outputFrame = frame;
                            break;

                        case EffectState::STATIC:
                            outputFrame = staticEffect.generate();
                            if (currentTime - stateStartTime >= config.staticDuration) {
                                effectState = EffectState::MATRIX;
                                stateStartTime = currentTime;
                                matrixEffect.reset();
                                std::cout << "Showing matrix effect...\n";
                            }
                            break;

                        case EffectState::MATRIX:
                            matrixEffect.update(currentTime);
                            if (config.overlay) {
                                outputFrame = matrixEffect.renderOverlay(frame, 0.9f);
                            } else {
                                outputFrame = matrixEffect.render();
                            }
                            if (currentTime - stateStartTime >= config.effectDuration) {
                                effectState = EffectState::PASSTHROUGH;
                                cycleCount++;

                                if (config.cycles > 0 && cycleCount >= config.cycles) {
                                    effectsFinished = true;
                                    std::cout << "Completed " << cycleCount << " cycle(s). Passthrough only from now on.\n";
                                } else {
                                    nextEffectTime = currentTime + randomIntervalMs(config.minInterval, config.maxInterval, rng);
                                    std::cout << "Returning to passthrough. Next effect in "
                                              << formatTime(nextEffectTime - currentTime) << "\n";
                                }
                            }
                            break;
                    }
                }
                break;
        }

        // Write to virtual camera
        if (!outputFrame.empty()) {
            output.writeFrame(outputFrame);
        }

        // Frame rate control
        int frameDelayMs = static_cast<int>(1000.0 / fps);
        if (frameDelayMs < 1) frameDelayMs = 1;
        cv::waitKey(frameDelayMs);
    }

    std::cout << "\nShutting down...\n";
    camera.close();
    output.close();

    return 0;
}
