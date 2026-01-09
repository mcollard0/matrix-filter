#include "virtual_output.h"
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>

VirtualOutput::~VirtualOutput() {
    close();
}

bool VirtualOutput::open(const std::string& device, int width, int height, double /*fps*/) {
    close();

    device_ = device;

    // Check if device exists
    if (!std::filesystem::exists(device)) {
        std::cerr << "Virtual camera device not found: " << device << std::endl;
        std::cerr << "Make sure v4l2loopback is loaded:" << std::endl;
        std::cerr << "  sudo modprobe v4l2loopback devices=1 video_nr=2 card_label=\"Matrix Filter\"" << std::endl;
        return false;
    }

    // Open device
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "Failed to open " << device << ": " << strerror(errno) << std::endl;
        return false;
    }

    // Set format - use YUYV which is widely supported
    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = width * 2;
    fmt.fmt.pix.sizeimage = width * height * 2;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        std::cerr << "Failed to set format: " << strerror(errno) << std::endl;
        close();
        return false;
    }

    // Store the actual negotiated values (driver may have adjusted them)
    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;
    bytesPerLine_ = fmt.fmt.pix.bytesperline;
    frameSize_ = fmt.fmt.pix.sizeimage;

    // Verify format was accepted
    if (width_ != width || height_ != height) {
        std::cout << "Note: Virtual camera negotiated " << width_ << "x" << height_
                  << " (requested " << width << "x" << height << ")" << std::endl;
    }

    std::cout << "Opened virtual camera: " << device << " (" << width_ << "x" << height_ << " YUYV)" << std::endl;
    return true;
}

void VirtualOutput::writeFrame(const cv::Mat& frame) {
    if (fd_ < 0 || frame.empty()) {
        return;
    }

    cv::Mat resized;
    if (frame.cols != width_ || frame.rows != height_) {
        cv::resize(frame, resized, cv::Size(width_, height_));
    } else {
        resized = frame;
    }

    // Convert BGR to YUYV
    cv::Mat yuyv;
    cv::cvtColor(resized, yuyv, cv::COLOR_BGR2YUV_YUYV);

    // Write exactly the buffer size (not the pre-computed frameSize_)
    size_t bytesToWrite = yuyv.total() * yuyv.elemSize();

    // Sanity check: warn if sizes don't match
    if (bytesToWrite != frameSize_) {
        std::cerr << "Warning: Buffer size mismatch (buffer=" << bytesToWrite
                  << ", expected=" << frameSize_ << ")" << std::endl;
    }

    ssize_t written = write(fd_, yuyv.data, bytesToWrite);
    if (written < 0) {
        std::cerr << "Write error: " << strerror(errno) << std::endl;
    }
}

bool VirtualOutput::isOpened() const {
    return fd_ >= 0;
}

void VirtualOutput::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
