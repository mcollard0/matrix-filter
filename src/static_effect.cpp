#include "static_effect.h"
#include <random>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

static std::mt19937 rng(std::random_device{}());

// Cache directory base path
static const std::string CACHE_BASE = "/tmp/matrix-filter-static";

// Font paths to try
static const char* fontPaths[] = {
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/OTF/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    nullptr
};

static uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

StaticEffect::~StaticEffect() {
    if (ftFace_) FT_Done_Face(ftFace_);
    if (ftLibrary_) FT_Done_FreeType(ftLibrary_);
}

void StaticEffect::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    cachedFrames_.clear();
    currentFrame_ = 0;
    frameCounter_ = 0;
    currentCharSize_ = 0;
    animationComplete_ = false;
    startTime_ = 0;

    // Initialize matrix characters (Katakana)
    matrixChars_.clear();
    for (wchar_t c = 0x30A0; c <= 0x30FF; ++c) {
        matrixChars_.push_back(c);
    }
    // Add some digits and symbols
    for (char c = '0'; c <= '9'; ++c) {
        matrixChars_.push_back(static_cast<wchar_t>(c));
    }

    // Initialize FreeType
    if (!ftLibrary_) {
        if (FT_Init_FreeType(&ftLibrary_) != 0) {
            std::cerr << "Failed to init FreeType for static effect\n";
            return;
        }
    }

    // Load font
    if (!fontLoaded_) {
        for (int i = 0; fontPaths[i]; ++i) {
            if (std::filesystem::exists(fontPaths[i])) {
                if (FT_New_Face(ftLibrary_, fontPaths[i], 0, &ftFace_) == 0) {
                    fontLoaded_ = true;
                    break;
                }
            }
        }
    }
}

void StaticEffect::resetForIdle() {
    // Growing static while waiting for camera (1px -> 5px over 10s)
    startTime_ = getCurrentTimeMs();
    currentCharSize_ = MIN_CHAR_SIZE;
    animationComplete_ = false;
    cachedFrames_.clear();
    buildCachedFrames(currentCharSize_);
}

void StaticEffect::resetForEffect() {
    // Instant full-size static for effect sequence
    startTime_ = getCurrentTimeMs();
    currentCharSize_ = MAX_CHAR_SIZE;
    animationComplete_ = true;  // No animation
    cachedFrames_.clear();
    buildCachedFrames(currentCharSize_);
}

void StaticEffect::renderChar(cv::Mat& img, wchar_t ch, int x, int y, uchar brightness, int charSize) {
    if (!fontLoaded_ || !ftFace_) return;

    FT_Set_Pixel_Sizes(ftFace_, 0, charSize);

    FT_UInt glyphIndex = FT_Get_Char_Index(ftFace_, ch);
    if (FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_RENDER) != 0) return;

    FT_Bitmap& bitmap = ftFace_->glyph->bitmap;
    int bx = ftFace_->glyph->bitmap_left;
    int by = ftFace_->glyph->bitmap_top;

    for (unsigned int row = 0; row < bitmap.rows; ++row) {
        for (unsigned int col = 0; col < bitmap.width; ++col) {
            int px = x + bx + col;
            int py = y - by + row;

            if (px >= 0 && px < img.cols && py >= 0 && py < img.rows) {
                uchar alpha = bitmap.buffer[row * bitmap.pitch + col];
                if (alpha > 0) {
                    uchar val = static_cast<uchar>((alpha * brightness) / 255);
                    cv::Vec3b& pixel = img.at<cv::Vec3b>(py, px);
                    pixel[0] = pixel[1] = pixel[2] = std::max(pixel[0], val);
                }
            }
        }
    }
}

std::string StaticEffect::getCacheDir(int charSize) const {
    std::ostringstream oss;
    oss << CACHE_BASE << "/" << width_ << "x" << height_ << "_size" << charSize;
    return oss.str();
}

bool StaticEffect::loadCachedFramesFromDisk(int charSize) {
    std::string cacheDir = getCacheDir(charSize);

    if (!std::filesystem::exists(cacheDir)) {
        return false;
    }

    cachedFrames_.clear();
    cachedFrames_.reserve(CACHE_SIZE);

    for (int i = 0; i < CACHE_SIZE; ++i) {
        std::ostringstream path;
        path << cacheDir << "/frame_" << std::setfill('0') << std::setw(3) << i << ".png";

        cv::Mat frame = cv::imread(path.str(), cv::IMREAD_COLOR);
        if (frame.empty() || frame.cols != width_ || frame.rows != height_) {
            cachedFrames_.clear();
            return false;
        }
        cachedFrames_.push_back(frame);
    }

    return true;
}

void StaticEffect::saveCachedFramesToDisk(int charSize) {
    std::string cacheDir = getCacheDir(charSize);

    try {
        std::filesystem::create_directories(cacheDir);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create cache directory: " << e.what() << std::endl;
        return;
    }

    for (size_t i = 0; i < cachedFrames_.size(); ++i) {
        std::ostringstream path;
        path << cacheDir << "/frame_" << std::setfill('0') << std::setw(3) << i << ".png";
        cv::imwrite(path.str(), cachedFrames_[i]);
    }
}

void StaticEffect::buildCachedFrames(int charSize) {
    // Try to load from disk cache first
    if (loadCachedFramesFromDisk(charSize)) {
        return;  // Successfully loaded from cache
    }

    cachedFrames_.clear();
    cachedFrames_.reserve(CACHE_SIZE);

    std::uniform_int_distribution<size_t> charDist(0, matrixChars_.size() - 1);
    std::uniform_int_distribution<int> brightDist(100, 255);
    std::uniform_int_distribution<int> bandCountDist(5, 25);
    std::uniform_int_distribution<int> bandHeightDist(2, 40);

    int charWidth = std::max(charSize, 2);
    int charHeight = charSize + 1;
    int cols = width_ / charWidth;
    int rows = height_ / charHeight;

    for (int f = 0; f < CACHE_SIZE; ++f) {
        cv::Mat frame = cv::Mat::zeros(height_, width_, CV_8UC3);

        // Fill with random matrix characters at varying brightness
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                wchar_t ch = matrixChars_[charDist(rng)];
                uchar brightness = static_cast<uchar>(brightDist(rng));
                int x = col * charWidth;
                int y = (row + 1) * charHeight;
                renderChar(frame, ch, x, y, brightness, charSize);
            }
        }

        // Add horizontal bands for TV effect
        int numBands = bandCountDist(rng);
        std::uniform_int_distribution<int> bandStartDist(0, height_ - 1);

        for (int b = 0; b < numBands; ++b) {
            int startY = bandStartDist(rng);
            int bandHeight = std::min(bandHeightDist(rng), height_ - startY);
            bool isBright = (rng() % 2) == 0;

            for (int y = startY; y < startY + bandHeight && y < height_; ++y) {
                cv::Vec3b* rowPtr = frame.ptr<cv::Vec3b>(y);
                for (int x = 0; x < width_; ++x) {
                    if (isBright) {
                        for (int c = 0; c < 3; ++c) {
                            rowPtr[x][c] = std::min(255, rowPtr[x][c] + 60);
                        }
                    } else {
                        for (int c = 0; c < 3; ++c) {
                            rowPtr[x][c] = static_cast<uchar>(rowPtr[x][c] * 0.5);
                        }
                    }
                }
            }
        }

        // Subtle scanline effect
        for (int y = 0; y < height_; y += 2) {
            cv::Vec3b* rowPtr = frame.ptr<cv::Vec3b>(y);
            for (int x = 0; x < width_; ++x) {
                for (int c = 0; c < 3; ++c) {
                    rowPtr[x][c] = static_cast<uchar>(rowPtr[x][c] * 0.9);
                }
            }
        }

        cachedFrames_.push_back(frame);
    }

    // Save to disk for future runs
    saveCachedFramesToDisk(charSize);
}

cv::Mat StaticEffect::generate() {
    // Initialize on first call if not reset
    if (startTime_ == 0) {
        resetForIdle();
    }

    // If no cached frames (font failed), fall back to noise
    if (cachedFrames_.empty()) {
        cv::Mat noise(height_, width_, CV_8UC1);
        cv::randu(noise, cv::Scalar(0), cv::Scalar(255));
        cv::Mat buffer;
        cv::cvtColor(noise, buffer, cv::COLOR_GRAY2BGR);
        return buffer;
    }

    // Calculate current character size based on elapsed time
    if (!animationComplete_) {
        uint64_t elapsed = getCurrentTimeMs() - startTime_;

        if (elapsed >= GROW_DURATION_MS) {
            // Animation complete, stay at max size
            if (currentCharSize_ != MAX_CHAR_SIZE) {
                currentCharSize_ = MAX_CHAR_SIZE;
                buildCachedFrames(currentCharSize_);
            }
            animationComplete_ = true;
        } else {
            // Interpolate size from MIN to MAX over duration
            float progress = static_cast<float>(elapsed) / GROW_DURATION_MS;
            int targetSize = MIN_CHAR_SIZE + static_cast<int>(progress * (MAX_CHAR_SIZE - MIN_CHAR_SIZE));

            // Rebuild cache if size changed
            if (targetSize != currentCharSize_) {
                currentCharSize_ = targetSize;
                buildCachedFrames(currentCharSize_);
            }
        }
    }

    // Cycle through cached frames slowly
    frameCounter_++;
    if (frameCounter_ >= framesPerSwitch_) {
        frameCounter_ = 0;
        currentFrame_ = (currentFrame_ + 1) % cachedFrames_.size();
    }

    return cachedFrames_[currentFrame_].clone();
}
