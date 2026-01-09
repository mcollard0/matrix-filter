#include "camera_capture.h"
#include <iostream>
#include <filesystem>

CameraCapture::~CameraCapture() {
    close();
}

bool CameraCapture::detectCamera() {
    // Try /dev/video0 through /dev/video9
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/video" + std::to_string(i);
        if (std::filesystem::exists(device)) {
            cv::VideoCapture testCap(device, cv::CAP_V4L2);
            if (testCap.isOpened()) {
                // Check if this is a capture device (not output-only)
                cv::Mat testFrame;
                if (testCap.read(testFrame) && !testFrame.empty()) {
                    testCap.release();
                    return open(device);
                }
                testCap.release();
            }
        }
    }
    std::cerr << "No camera detected" << std::endl;
    return false;
}

bool CameraCapture::open(const std::string& device) {
    close();

    device_ = device;
    cap_.open(device, cv::CAP_V4L2);

    if (!cap_.isOpened()) {
        std::cerr << "Failed to open camera: " << device << std::endl;
        return false;
    }

    // Get camera properties
    width_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = cap_.get(cv::CAP_PROP_FPS);

    if (fps_ <= 0 || fps_ > 120) {
        fps_ = 30.0;  // Default to 30 FPS if invalid
    }

    std::cout << "Opened camera: " << device << std::endl;
    std::cout << "Resolution: " << width_ << "x" << height_ << " @ " << fps_ << " FPS" << std::endl;

    return true;
}

cv::Mat CameraCapture::captureFrame() {
    cv::Mat frame;
    if (cap_.isOpened()) {
        cap_ >> frame;
    }
    return frame;
}

void CameraCapture::getResolution(int& width, int& height) const {
    width = width_;
    height = height_;
}

double CameraCapture::getFPS() const {
    return fps_;
}

bool CameraCapture::isOpened() const {
    return cap_.isOpened();
}

void CameraCapture::close() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}
