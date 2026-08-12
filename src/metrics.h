#pragma once

#include "common.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace sysglance {

class MetricService {
public:
    struct CpuState {
        std::uint64_t idle = 0;
        std::uint64_t kernel = 0;
        std::uint64_t user = 0;
        bool initialized = false;
    };

    struct NetworkState {
        struct InterfaceCounters {
            std::uint64_t receive = 0;
            std::uint64_t transmit = 0;
        };

        std::unordered_map<std::uint64_t, InterfaceCounters> interfaces;
        std::uint64_t timestamp = 0;
        bool initialized = false;
    };

    MetricService();
    ~MetricService();

    MetricService(const MetricService&) = delete;
    MetricService& operator=(const MetricService&) = delete;

    bool Start(int intervalMs, HWND notificationWindow);
    void Stop();
    void SetInterval(int intervalMs);
    void SetNetworkSelection(std::uint64_t luid, bool includeVirtualInterfaces);
    void SetGpuSelection(std::uint64_t luid);
    std::shared_ptr<const MetricSnapshot> Snapshot() const;
    std::vector<NetworkInterfaceInfo> NetworkInterfaces() const;
    std::vector<GpuAdapterInfo> GpuAdapters() const;

private:
    void Run();
    void Sample(MetricSnapshot& snapshot);

    std::atomic<bool> running_{false};
    std::atomic<int> intervalMs_{1000};
    HWND notificationWindow_ = nullptr;
    std::thread worker_;
    mutable std::mutex wakeMutex_;
    std::condition_variable wakeCondition_;
    std::atomic<std::shared_ptr<MetricSnapshot>> snapshot_{std::make_shared<MetricSnapshot>()};

    CpuState cpu_;
    NetworkState network_;

    std::atomic<std::uint64_t> selectedNetworkLuid_{0};
    std::atomic<bool> includeVirtualNetworkInterfaces_{false};
    std::atomic<std::uint64_t> networkSelectionGeneration_{1};
    std::uint64_t appliedNetworkSelectionGeneration_ = 0;
    std::atomic<std::uint64_t> selectedGpuLuid_{0};
    mutable std::mutex devicesMutex_;
    std::vector<NetworkInterfaceInfo> networkInterfaces_;
    std::vector<GpuAdapterInfo> gpuAdapters_;

    struct GpuState;
    std::unique_ptr<GpuState> gpu_;
};

}  // namespace sysglance
