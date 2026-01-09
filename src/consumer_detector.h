#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>

class ConsumerDetector {
public:
    explicit ConsumerDetector(const std::string& devicePath)
        : devicePath_(devicePath) {
        // Get the device's inode for comparison
        struct stat st;
        if (stat(devicePath.c_str(), &st) == 0) {
            deviceInode_ = st.st_ino;
            deviceDev_ = st.st_rdev;
        }

        // Extract device name (e.g., "video2" from "/dev/video2")
        size_t lastSlash = devicePath.rfind('/');
        if (lastSlash != std::string::npos) {
            deviceName_ = devicePath.substr(lastSlash + 1);
        }
    }

    // Check if any process (other than us) has the virtual camera open
    bool hasConsumers() const {
        // Method 1: Check sysfs for reader count (faster if available)
        int sysfsCount = checkSysfs();
        if (sysfsCount >= 0) {
            // sysfs reports count, subtract 1 for our own writer
            return sysfsCount > 1;
        }

        // Method 2: Scan /proc for file descriptors (fallback)
        return checkProcFd();
    }

    // Get count of consumers (approximate)
    int getConsumerCount() const {
        int sysfsCount = checkSysfs();
        if (sysfsCount >= 0) {
            return std::max(0, sysfsCount - 1);  // Subtract our writer
        }
        return countProcFdConsumers();
    }

private:
    std::string devicePath_;
    std::string deviceName_;
    ino_t deviceInode_ = 0;
    dev_t deviceDev_ = 0;

    // Check sysfs for open count (v4l2loopback specific)
    int checkSysfs() const {
        // Try various sysfs paths that v4l2loopback might expose
        std::vector<std::string> paths = {
            "/sys/devices/virtual/video4linux/" + deviceName_ + "/open_count",
            "/sys/class/video4linux/" + deviceName_ + "/open_count",
            "/sys/devices/virtual/video4linux/" + deviceName_ + "/readers"
        };

        for (const auto& path : paths) {
            std::ifstream f(path);
            if (f.is_open()) {
                int count = 0;
                f >> count;
                return count;
            }
        }
        return -1;  // Not available
    }

    // Scan /proc/*/fd for processes with our device open
    bool checkProcFd() const {
        pid_t ourPid = getpid();
        DIR* procDir = opendir("/proc");
        if (!procDir) return false;

        struct dirent* entry;
        while ((entry = readdir(procDir)) != nullptr) {
            // Skip non-numeric entries
            if (entry->d_type != DT_DIR) continue;

            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0') continue;  // Not a number

            // Skip our own process
            if (pid == ourPid) continue;

            // Check this process's file descriptors
            std::string fdPath = "/proc/" + std::string(entry->d_name) + "/fd";
            DIR* fdDir = opendir(fdPath.c_str());
            if (!fdDir) continue;

            struct dirent* fdEntry;
            while ((fdEntry = readdir(fdDir)) != nullptr) {
                std::string linkPath = fdPath + "/" + fdEntry->d_name;
                char target[PATH_MAX];
                ssize_t len = readlink(linkPath.c_str(), target, sizeof(target) - 1);
                if (len > 0) {
                    target[len] = '\0';
                    if (std::string(target) == devicePath_) {
                        closedir(fdDir);
                        closedir(procDir);
                        return true;
                    }
                }
            }
            closedir(fdDir);
        }
        closedir(procDir);
        return false;
    }

    // Count consumers via /proc (more expensive)
    int countProcFdConsumers() const {
        pid_t ourPid = getpid();
        int count = 0;

        DIR* procDir = opendir("/proc");
        if (!procDir) return 0;

        struct dirent* entry;
        while ((entry = readdir(procDir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;

            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0') continue;
            if (pid == ourPid) continue;

            std::string fdPath = "/proc/" + std::string(entry->d_name) + "/fd";
            DIR* fdDir = opendir(fdPath.c_str());
            if (!fdDir) continue;

            struct dirent* fdEntry;
            while ((fdEntry = readdir(fdDir)) != nullptr) {
                std::string linkPath = fdPath + "/" + fdEntry->d_name;
                char target[PATH_MAX];
                ssize_t len = readlink(linkPath.c_str(), target, sizeof(target) - 1);
                if (len > 0) {
                    target[len] = '\0';
                    if (std::string(target) == devicePath_) {
                        count++;
                        break;  // Count each process once
                    }
                }
            }
            closedir(fdDir);
        }
        closedir(procDir);
        return count;
    }
};
