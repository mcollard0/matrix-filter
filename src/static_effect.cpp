#include "static_effect.h"

void StaticEffect::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    buffer_ = cv::Mat(height_, width_, CV_8UC3);
}

cv::Mat StaticEffect::generate() {
    // Generate random grayscale noise
    cv::Mat noise(height_, width_, CV_8UC1);
    cv::randu(noise, cv::Scalar(0), cv::Scalar(255));

    // Convert to BGR (grayscale noise in color format)
    cv::cvtColor(noise, buffer_, cv::COLOR_GRAY2BGR);

    // Add some scanline effect for authentic TV static look
    for (int y = 0; y < height_; y += 2) {
        buffer_.row(y) *= 0.7;
    }

    return buffer_.clone();
}
