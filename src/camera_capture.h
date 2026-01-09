#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class CameraCapture {
public:
    CameraCapture() = default;
    ~CameraCapture();

    bool detectCamera();
    bool open(const std::string& device);
    cv::Mat captureFrame();
    void getResolution(int& width, int& height) const;
    double getFPS() const;
    bool isOpened() const;
    void close();

private:
    cv::VideoCapture cap_;
    int width_ = 0;
    int height_ = 0;
    double fps_ = 30.0;
    std::string device_;
};
