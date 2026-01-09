#pragma once

#include <opencv2/opencv.hpp>

class StaticEffect {
public:
    StaticEffect() = default;

    void initialize(int width, int height);
    cv::Mat generate();

private:
    int width_ = 0;
    int height_ = 0;
    cv::Mat buffer_;
};
