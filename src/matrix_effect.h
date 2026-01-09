#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <random>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

struct MatrixColumn {
    std::vector<int> charIndices;   // Indices into character set
    int headPosition;                // Y position of leading character (in pixels)
    int speed;                       // Pixels per update
    int trailLength;                 // Number of characters in trail
    uint64_t lastUpdate;             // Last update timestamp
};

class MatrixEffect {
public:
    MatrixEffect();
    ~MatrixEffect();

    bool initialize(int width, int height);
    void update(uint64_t currentTimeMs);
    cv::Mat render();
    void reset();

private:
    void loadCharacters();
    bool initFreeType();
    void renderGlyph(cv::Mat& img, const std::string& ch, int x, int y, cv::Scalar color);
    void initializeColumn(MatrixColumn& col);
    int randomChar();
    cv::Scalar getCharColor(int distanceFromHead) const;

    int width_ = 0;
    int height_ = 0;
    int charWidth_ = 10;      // Narrower columns for more density
    int charHeight_ = 16;     // Smaller chars
    int numColumns_ = 0;

    std::vector<MatrixColumn> columns_;
    std::vector<std::string> characters_;
    cv::Mat buffer_;

    // FreeType
    FT_Library ftLibrary_ = nullptr;
    FT_Face ftFace_ = nullptr;
    bool ftInitialized_ = false;

    std::mt19937 rng_;
    std::uniform_int_distribution<int> speedDist_{4, 10};   // Slower speeds
    std::uniform_int_distribution<int> trailDist_{8, 30};   // Longer trails

    uint64_t lastUpdateTime_ = 0;
    static constexpr int UPDATE_INTERVAL_MS = 50;  // ~20 FPS for smoother animation
};
