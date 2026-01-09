#pragma once

#include <string>
#include <stdexcept>
#include <cctype>
#include <algorithm>

// Parse a time string like "500ms", "5s", "2m", "1h" into milliseconds
// Supported suffixes:
//   ms, milli, millisecond, milliseconds
//   s, sec, secs, second, seconds
//   m, min, minute, minutes
//   h, hour, hours
// If no suffix, assumes milliseconds
inline uint64_t parseTime(const std::string& input) {
    if (input.empty()) {
        throw std::invalid_argument("Empty time string");
    }

    // Find where the number ends and unit begins
    size_t unitStart = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (!std::isdigit(input[i])) {
            unitStart = i;
            break;
        }
        unitStart = i + 1;
    }

    if (unitStart == 0) {
        throw std::invalid_argument("Time string must start with a number: " + input);
    }

    uint64_t value = std::stoull(input.substr(0, unitStart));

    if (unitStart >= input.size()) {
        // No unit specified, assume milliseconds
        return value;
    }

    std::string unit = input.substr(unitStart);
    // Convert to lowercase
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit == "ms" || unit == "milli" || unit == "millisecond" || unit == "milliseconds") {
        return value;
    } else if (unit == "s" || unit == "sec" || unit == "secs" || unit == "second" || unit == "seconds") {
        return value * 1000;
    } else if (unit == "m" || unit == "min" || unit == "minute" || unit == "minutes") {
        return value * 60 * 1000;
    } else if (unit == "h" || unit == "hour" || unit == "hours") {
        return value * 60 * 60 * 1000;
    } else {
        throw std::invalid_argument("Unknown time unit: " + unit + " (use ms, s, m, or h)");
    }
}

// Format milliseconds as a human-readable string
inline std::string formatTime(uint64_t ms) {
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000) + "s";
    } else if (ms < 3600000) {
        return std::to_string(ms / 60000) + "m";
    } else {
        return std::to_string(ms / 3600000) + "h";
    }
}
