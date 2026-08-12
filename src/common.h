#pragma once

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace sysglance {

enum class DisplayMode : int {
    Tray = 0,
    Taskbar = 1,
    Hud = 2,
};

struct MetricSnapshot {
    std::uint64_t timestampMs = 0;

    double cpuPercent = 0.0;

    std::uint64_t memoryUsedBytes = 0;
    std::uint64_t memoryTotalBytes = 0;
    double memoryPercent = 0.0;

    double gpuUtilPercent = 0.0;
    std::uint64_t gpuMemoryUsedBytes = 0;
    std::uint64_t gpuMemoryTotalBytes = 0;

    double networkDownloadBytesPerSecond = 0.0;
    double networkUploadBytesPerSecond = 0.0;

    bool gpuAvailable = false;
    bool gpuMemoryAvailable = false;
    bool networkAvailable = false;
    // False during a baseline/reset interval. Renderers should show N/A instead of a spike.
    bool networkReady = false;
};

struct NetworkInterfaceInfo {
    std::uint64_t luid = 0;
    std::wstring name;
    std::wstring description;
    bool connected = false;
    bool physical = false;
    bool included = false;
};

struct GpuAdapterInfo {
    std::uint64_t luid = 0;
    std::wstring name;
    std::uint64_t memoryCapacityBytes = 0;
};

struct AppConfig {
    DisplayMode displayMode = DisplayMode::Tray;
    int refreshIntervalMs = 1000;
    int fontSize = 12;

    bool showCpu = true;
    bool showMemory = true;
    bool memoryShowPercent = false;
    bool gpuMemoryShowPercent = false;
    bool showGpu = true;
    bool showNetwork = true;
    bool showPercentDecimal = true;
    bool showNetworkArrows = true;
    bool includeVirtualNetworkInterfaces = false;

    // Zero means aggregate all eligible devices.
    std::uint64_t selectedNetworkLuid = 0;
    std::uint64_t selectedGpuLuid = 0;

    bool darkTheme = true;
    bool hudLocked = false;
    bool hudClickThrough = false;
    bool hudNetworkOnly = false;
    bool autoStart = false;
    int hudOpacity = 90;
    int hudWidthDip = 360;
    int hudHeightDip = 34;
    COLORREF hudBorderColor = RGB(255, 96, 0);
    COLORREF hudTextColor = RGB(255, 255, 255);
    COLORREF hudBackgroundColor = RGB(10, 13, 18);
    int hudBorderThicknessTenths = 5;
    int hudColorPreset = 0;

    RECT hudRect{0, 0, 0, 0};
};

inline std::uint64_t SaturatingDelta(std::uint64_t current, std::uint64_t previous) {
    return current >= previous ? current - previous : 0;
}

inline std::uint64_t BytesPerSecond(std::uint64_t current,
                                     std::uint64_t previous,
                                     std::uint64_t elapsedMs) {
    if (elapsedMs == 0) {
        return 0;
    }
    const auto delta = SaturatingDelta(current, previous);
    return static_cast<std::uint64_t>(
        (static_cast<long double>(delta) * 1000.0L) / static_cast<long double>(elapsedMs));
}

inline double PreciseBytesPerSecond(std::uint64_t current,
                                    std::uint64_t previous,
                                    std::uint64_t elapsedMs) {
    if (elapsedMs == 0) {
        return 0.0;
    }
    const auto delta = SaturatingDelta(current, previous);
    return static_cast<double>(static_cast<long double>(delta) * 1000.0L /
                               static_cast<long double>(elapsedMs));
}

inline bool IsKnownRefreshInterval(int value) {
    return value == 500 || value == 1000 || value == 2000;
}

// Compact network rate for HUD/tray. Never uses raw bytes.
// Below 100 KiB/s keeps one-decimal KB (0.0KB .. 99.9KB); at/above that threshold
// switches to MB so ~100 KiB/s reads as 0.1MB. Number width 6, unit width 3.
inline std::wstring FormatNetworkRate(double bytesPerSecond) {
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * kKiB;
    constexpr double kSwitchToMiBBytes = 100.0 * kKiB;

    bytesPerSecond = std::max(0.0, bytesPerSecond);
    const bool useMib = bytesPerSecond >= kSwitchToMiBBytes;
    const double value = useMib ? bytesPerSecond / kMiB : bytesPerSecond / kKiB;

    std::wstringstream stream;
    stream << std::fixed << std::setprecision(value >= 1000.0 ? 0 : 1) << value;
    std::wstring number = stream.str();
    if (number.size() > 6) {
        number = L"99999+";
    } else if (number.size() < 6) {
        number.insert(0, 6 - number.size(), L' ');
    }
    return number + (useMib ? L"MB " : L"KB ");
}

}  // namespace sysglance
