#include "camera_capture.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

CameraCapture::~CameraCapture() {
    close();
}

std::vector<ResolutionMode> CameraCapture::queryResolutions(const std::string& device) {
    std::set<ResolutionMode> resolutions;

    int fd = ::open(device.c_str(), O_RDONLY);
    if (fd < 0) {
        return {};
    }

    // Query supported formats
    struct v4l2_fmtdesc fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        // For each format, enumerate frame sizes
        struct v4l2_frmsizeenum frmsize{};
        frmsize.pixel_format = fmt.pixelformat;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                resolutions.insert({
                    static_cast<int>(frmsize.discrete.width),
                    static_cast<int>(frmsize.discrete.height)
                });
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                       frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                // Add common resolutions within the range
                int common[][2] = {
                    {640, 480}, {800, 600}, {1024, 768}, {1280, 720},
                    {1280, 960}, {1920, 1080}, {2560, 1440}, {3840, 2160}
                };
                for (auto& res : common) {
                    if (res[0] >= static_cast<int>(frmsize.stepwise.min_width) &&
                        res[0] <= static_cast<int>(frmsize.stepwise.max_width) &&
                        res[1] >= static_cast<int>(frmsize.stepwise.min_height) &&
                        res[1] <= static_cast<int>(frmsize.stepwise.max_height)) {
                        resolutions.insert({res[0], res[1]});
                    }
                }
                break;
            }
            frmsize.index++;
        }
        fmt.index++;
    }

    ::close(fd);

    // Convert set to sorted vector
    std::vector<ResolutionMode> result(resolutions.begin(), resolutions.end());
    std::sort(result.begin(), result.end());
    return result;
}

bool CameraCapture::setResolution(Resolution resPref) {
    auto resolutions = queryResolutions(device_);

    if (resolutions.empty()) {
        std::cerr << "Could not query camera resolutions" << std::endl;
        return false;
    }

    // Print available resolutions
    std::cout << "Available resolutions:" << std::endl;
    for (const auto& res : resolutions) {
        std::cout << "  " << res.width << "x" << res.height << std::endl;
    }

    // Select resolution based on preference
    ResolutionMode selected;
    size_t count = resolutions.size();

    switch (resPref) {
        case Resolution::LOW:
            selected = resolutions.front();
            break;
        case Resolution::HIGH:
            selected = resolutions.back();
            break;
        case Resolution::MEDIUM:
            if (count <= 2) {
                selected = resolutions.back();  // Best if only 1-2 options
            } else {
                // Middle with ceil: (count-1)/2 rounded up
                size_t idx = (count + 1) / 2 - 1;
                selected = resolutions[idx];
            }
            break;
    }

    std::cout << "Selected resolution: " << selected.width << "x" << selected.height << std::endl;

    // Set the resolution
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, selected.width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, selected.height);

    // Verify what we actually got
    width_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));

    if (width_ != selected.width || height_ != selected.height) {
        std::cout << "Note: Camera set to " << width_ << "x" << height_
                  << " (requested " << selected.width << "x" << selected.height << ")" << std::endl;
    }

    return true;
}

bool CameraCapture::detectCamera(Resolution resPref) {
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
                    return open(device, resPref);
                }
                testCap.release();
            }
        }
    }
    std::cerr << "No camera detected" << std::endl;
    return false;
}

bool CameraCapture::open(const std::string& device, Resolution resPref) {
    close();

    device_ = device;
    cap_.open(device, cv::CAP_V4L2);

    if (!cap_.isOpened()) {
        std::cerr << "Failed to open camera: " << device << std::endl;
        return false;
    }

    std::cout << "Opened camera: " << device << std::endl;

    // Set resolution based on preference
    if (!setResolution(resPref)) {
        // Fall back to default
        width_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
        height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    }

    fps_ = cap_.get(cv::CAP_PROP_FPS);
    if (fps_ <= 0 || fps_ > 120) {
        fps_ = 30.0;  // Default to 30 FPS if invalid
    }

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
