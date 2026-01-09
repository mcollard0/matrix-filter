#pragma once

#include <string>

struct Config {
    std::string inputDevice = "";           // Empty = auto-detect
    std::string outputDevice = "/dev/video2";
    int minInterval = 1;                    // minutes
    int maxInterval = 60;                   // minutes
    int effectDuration = 5000;              // milliseconds
    int staticFrames = 10;                  // frame count
    bool testMode = false;                  // Trigger effect immediately
    int cycles = 0;                         // Number of effect cycles (0 = infinite)
};

enum class EffectState {
    PASSTHROUGH,
    STATIC,
    MATRIX
};
