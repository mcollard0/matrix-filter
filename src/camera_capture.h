#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "config.h"

struct ResolutionMode {
    int width;
    int height;

    bool operator<(const ResolutionMode& other) const {
        return (width * height) < (other.width * other.height);
    }
    bool operator==(const ResolutionMode& other) const {
        return width == other.width && height == other.height;
    }
};

class CameraCapture {
public:
    CameraCapture() = default;
    ~CameraCapture();

    bool detectCamera(Resolution resPref = Resolution::HIGH);
    bool open(const std::string& device, Resolution resPref = Resolution::HIGH);
    cv::Mat captureFrame();
    void getResolution(int& width, int& height) const;
    double getFPS() const;
    bool isOpened() const;
    void close();

    static std::vector<ResolutionMode> queryResolutions(const std::string& device);

private:
    bool setResolution(Resolution resPref);

    cv::VideoCapture cap_;
    int width_ = 0;
    int height_ = 0;
    double fps_ = 30.0;
    std::string device_;
};
