#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class VirtualOutput {
public:
    VirtualOutput() = default;
    ~VirtualOutput();

    bool open(const std::string& device, int width, int height, double fps);
    void writeFrame(const cv::Mat& frame);
    bool isOpened() const;
    void close();

    // Get the actual negotiated resolution (may differ from requested)
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    int fd_ = -1;
    int width_ = 0;
    int height_ = 0;
    size_t bytesPerLine_ = 0;
    size_t frameSize_ = 0;
    std::string device_;
};
