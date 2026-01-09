#pragma once

#include <string>
#include <cstdint>

enum class Resolution {
    LOW,
    MEDIUM,
    HIGH
};

struct Config {
    std::string inputDevice = "";           // Empty = auto-detect
    std::string outputDevice = "/dev/video2";
    uint64_t minInterval = 60000;           // milliseconds (default 1 minute)
    uint64_t maxInterval = 3600000;         // milliseconds (default 60 minutes)
    uint64_t effectDuration = 5000;         // milliseconds
    uint64_t staticDuration = 300;          // milliseconds
    uint64_t startDelay = 0;                // milliseconds (0 = use random interval)
    bool testMode = false;                  // Trigger effect immediately (startDelay=0)
    int cycles = 0;                         // Number of effect cycles (0 = infinite)
    Resolution resolution = Resolution::HIGH;  // Camera resolution preference
    bool onDemand = true;                   // Only open camera when virtual camera has consumers
    uint64_t cameraPollInterval = 1000;     // ms between camera availability checks
};

enum class EffectState {
    PASSTHROUGH,
    STATIC,
    MATRIX
};

enum class CameraState {
    IDLE,           // No consumers, camera closed
    CONNECTING,     // Consumers present, trying to open camera
    ACTIVE,         // Camera open and working
    UNAVAILABLE     // Camera busy/unavailable, polling
};
