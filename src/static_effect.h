#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

class StaticEffect {
public:
    StaticEffect() = default;
    ~StaticEffect();

    void initialize(int width, int height);
    void resetForIdle();   // Growing static while waiting for camera
    void resetForEffect(); // Instant full-size static for effect sequence
    cv::Mat generate();

private:
    void buildCachedFrames(int charSize);
    void renderChar(cv::Mat& img, wchar_t ch, int x, int y, uchar brightness, int charSize);

    int width_ = 0;
    int height_ = 0;

    // Cached frames
    std::vector<cv::Mat> cachedFrames_;
    size_t currentFrame_ = 0;
    int frameCounter_ = 0;
    int framesPerSwitch_ = 3;  // Show each cached frame for N generate() calls

    // Size animation
    uint64_t startTime_ = 0;
    int currentCharSize_ = 0;
    bool animationComplete_ = false;

    static constexpr int CACHE_SIZE = 20;
    static constexpr int MIN_CHAR_SIZE = 1;   // Start tiny
    static constexpr int MAX_CHAR_SIZE = 5;   // Final size
    static constexpr uint64_t GROW_DURATION_MS = 10000;  // 10 seconds

    // FreeType
    FT_Library ftLibrary_ = nullptr;
    FT_Face ftFace_ = nullptr;
    bool fontLoaded_ = false;

    // Matrix characters (Katakana + digits)
    std::vector<wchar_t> matrixChars_;
};
