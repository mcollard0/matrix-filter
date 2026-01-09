#include "matrix_effect.h"
#include <iostream>
#include <codecvt>
#include <locale>

MatrixEffect::MatrixEffect() : rng_(std::random_device{}()) {
    loadCharacters();
}

MatrixEffect::~MatrixEffect() {
    if (ftFace_) {
        FT_Done_Face(ftFace_);
    }
    if (ftLibrary_) {
        FT_Done_FreeType(ftLibrary_);
    }
}

void MatrixEffect::loadCharacters() {
    // Half-width Katakana (like the original Matrix)
    characters_ = {
        "ｱ", "ｲ", "ｳ", "ｴ", "ｵ", "ｶ", "ｷ", "ｸ", "ｹ", "ｺ",
        "ｻ", "ｼ", "ｽ", "ｾ", "ｿ", "ﾀ", "ﾁ", "ﾂ", "ﾃ", "ﾄ",
        "ﾅ", "ﾆ", "ﾇ", "ﾈ", "ﾉ", "ﾊ", "ﾋ", "ﾌ", "ﾍ", "ﾎ",
        "ﾏ", "ﾐ", "ﾑ", "ﾒ", "ﾓ", "ﾔ", "ﾕ", "ﾖ", "ﾗ", "ﾘ",
        "ﾙ", "ﾚ", "ﾛ", "ﾜ", "ﾝ",
        // Numbers and symbols
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        ":", ".", "=", "*", "+", "-", "<", ">", "|",
        // Some Latin characters (Matrix style)
        "Z", "Y", "X", "W", "V", "U", "T", "S", "R", "Q"
    };
}

bool MatrixEffect::initFreeType() {
    if (ftInitialized_) return true;

    if (FT_Init_FreeType(&ftLibrary_)) {
        std::cerr << "Failed to initialize FreeType" << std::endl;
        return false;
    }

    // Try to load a Japanese font
    const char* fontPaths[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/OTF/NotoSansCJK-Regular.ttc",
        nullptr
    };

    for (int i = 0; fontPaths[i] != nullptr; ++i) {
        if (FT_New_Face(ftLibrary_, fontPaths[i], 0, &ftFace_) == 0) {
            std::cout << "Loaded font: " << fontPaths[i] << std::endl;
            FT_Set_Pixel_Sizes(ftFace_, 0, charHeight_ - 2);  // Font size matches char height
            ftInitialized_ = true;
            return true;
        }
    }

    std::cerr << "Failed to load Japanese font" << std::endl;
    return false;
}

void MatrixEffect::renderGlyph(cv::Mat& img, const std::string& ch, int x, int y, cv::Scalar color) {
    if (!ftInitialized_ || x < 0 || y < 0) return;

    // Convert UTF-8 string to Unicode codepoint
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str;
    try {
        u32str = converter.from_bytes(ch);
    } catch (...) {
        return;
    }
    if (u32str.empty()) return;

    char32_t codepoint = u32str[0];

    // Load glyph
    FT_UInt glyphIndex = FT_Get_Char_Index(ftFace_, codepoint);
    if (glyphIndex == 0) return;

    if (FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_RENDER)) return;

    FT_GlyphSlot slot = ftFace_->glyph;
    FT_Bitmap& bitmap = slot->bitmap;

    // Draw the glyph
    int startX = x + slot->bitmap_left;
    int startY = y - slot->bitmap_top;

    for (unsigned int row = 0; row < bitmap.rows; ++row) {
        for (unsigned int col = 0; col < bitmap.width; ++col) {
            int px = startX + col;
            int py = startY + row;

            if (px >= 0 && px < img.cols && py >= 0 && py < img.rows) {
                unsigned char alpha = bitmap.buffer[row * bitmap.pitch + col];
                if (alpha > 0) {
                    cv::Vec3b& pixel = img.at<cv::Vec3b>(py, px);
                    float a = alpha / 255.0f;
                    pixel[0] = static_cast<uchar>(pixel[0] * (1 - a) + color[0] * a);
                    pixel[1] = static_cast<uchar>(pixel[1] * (1 - a) + color[1] * a);
                    pixel[2] = static_cast<uchar>(pixel[2] * (1 - a) + color[2] * a);
                }
            }
        }
    }
}

bool MatrixEffect::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    buffer_ = cv::Mat::zeros(height_, width_, CV_8UC3);

    if (!initFreeType()) {
        std::cerr << "Warning: FreeType init failed, matrix effect may not render correctly" << std::endl;
    }

    // Calculate number of columns based on character width
    numColumns_ = width_ / charWidth_;
    columns_.resize(numColumns_);

    // Initialize each column with random state
    for (auto& col : columns_) {
        initializeColumn(col);
        // Randomize initial head position for varied start
        col.headPosition = -(rng_() % height_);
    }

    lastUpdateTime_ = 0;
    return true;
}

void MatrixEffect::initializeColumn(MatrixColumn& col) {
    col.speed = speedDist_(rng_);
    col.trailLength = trailDist_(rng_);
    col.headPosition = -col.trailLength * charHeight_;
    col.lastUpdate = 0;

    // Fill with random characters
    col.charIndices.resize(col.trailLength);
    for (int& idx : col.charIndices) {
        idx = randomChar();
    }
}

int MatrixEffect::randomChar() {
    return rng_() % characters_.size();
}

cv::Scalar MatrixEffect::getCharColor(int distanceFromHead) const {
    if (distanceFromHead == 0) {
        // Head character is bright white-green
        return cv::Scalar(200, 255, 200);
    }

    // Fade from bright green to dark green
    int brightness = std::max(50, 255 - distanceFromHead * 15);
    return cv::Scalar(0, brightness, 0);
}

void MatrixEffect::update(uint64_t currentTimeMs) {
    if (lastUpdateTime_ == 0) {
        lastUpdateTime_ = currentTimeMs;
        return;
    }

    if (currentTimeMs - lastUpdateTime_ < UPDATE_INTERVAL_MS) {
        return;
    }

    lastUpdateTime_ = currentTimeMs;

    for (auto& col : columns_) {
        // Move column down
        col.headPosition += col.speed;

        // Randomly change some characters
        if (rng_() % 10 == 0 && !col.charIndices.empty()) {
            int changeIdx = rng_() % col.charIndices.size();
            col.charIndices[changeIdx] = randomChar();
        }

        // Reset column when it goes off screen
        if (col.headPosition > height_ + col.trailLength * charHeight_) {
            initializeColumn(col);
        }
    }
}

cv::Mat MatrixEffect::render() {
    // Clear to black
    buffer_.setTo(cv::Scalar(0, 0, 0));

    for (int colIdx = 0; colIdx < numColumns_; ++colIdx) {
        const auto& col = columns_[colIdx];
        int x = colIdx * charWidth_ + 2;

        for (int i = 0; i < static_cast<int>(col.charIndices.size()); ++i) {
            int y = col.headPosition - i * charHeight_;

            // Skip if outside visible area
            if (y < -charHeight_ || y > height_ + charHeight_) {
                continue;
            }

            cv::Scalar color = getCharColor(i);
            const std::string& ch = characters_[col.charIndices[i]];

            if (ftInitialized_) {
                renderGlyph(buffer_, ch, x, y, color);
            }
        }
    }

    return buffer_.clone();
}

void MatrixEffect::reset() {
    for (auto& col : columns_) {
        initializeColumn(col);
        col.headPosition = -(rng_() % height_);
    }
    lastUpdateTime_ = 0;
}
