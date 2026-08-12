#include "metrics.h"

#include <ws2ipdef.h>
#include <dxgi1_4.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <iptypes.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <cmath>
#include <cstddef>
#include <chrono>
#include <limits>

namespace sysglance {
namespace {

constexpr UINT kMetricsReadyMessage = WM_APP + 1;

std::uint64_t FileTimeValue(const FILETIME& value) {
    ULARGE_INTEGER result{};
    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

bool ReadSystemCpu(MetricService::CpuState& state, double& percent) {
    FILETIME idleTime{}, kernelTime{}, userTime{};
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return false;
    }

    const auto idle = FileTimeValue(idleTime);
    const auto kernel = FileTimeValue(kernelTime);
    const auto user = FileTimeValue(userTime);
    if (!state.initialized) {
        state = {idle, kernel, user, true};
        percent = 0.0;
        return true;
    }

    const auto idleDelta = SaturatingDelta(idle, state.idle);
    const auto totalDelta = SaturatingDelta(kernel + user, state.kernel + state.user);
    state = {idle, kernel, user, true};
    percent = totalDelta == 0
                  ? 0.0
                  : std::clamp((1.0 - static_cast<double>(idleDelta) /
                                        static_cast<double>(totalDelta)) * 100.0,
                               0.0, 100.0);
    return true;
}

bool ReadMemory(MetricSnapshot& snapshot) {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        return false;
    }
    snapshot.memoryTotalBytes = memory.ullTotalPhys;
    snapshot.memoryUsedBytes = memory.ullTotalPhys - memory.ullAvailPhys;
    snapshot.memoryPercent = static_cast<double>(memory.dwMemoryLoad);
    return true;
}

bool IsConnectedCandidate(const MIB_IF_ROW2& row) {
    return row.OperStatus == IfOperStatusUp && row.Type != IF_TYPE_SOFTWARE_LOOPBACK &&
           !row.InterfaceAndOperStatusFlags.FilterInterface &&
           !row.InterfaceAndOperStatusFlags.EndPointInterface &&
           !row.InterfaceAndOperStatusFlags.NotMediaConnected &&
           row.MediaConnectState == MediaConnectStateConnected;
}

bool IsPhysicalInterface(const MIB_IF_ROW2& row) {
    return row.InterfaceAndOperStatusFlags.HardwareInterface && row.Type != IF_TYPE_TUNNEL;
}

std::wstring FriendlyName(const MIB_IF_ROW2& row) {
    if (row.Alias[0] != L'\0') return row.Alias;
    return row.Description;
}

bool ReadNetwork(MetricService::NetworkState& state, MetricSnapshot& snapshot,
                 std::uint64_t selectedLuid, bool includeVirtual, std::uint64_t selectionGeneration,
                 std::uint64_t& appliedGeneration, int intervalMs,
                 std::vector<NetworkInterfaceInfo>& inventory) {
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || table == nullptr) {
        return false;
    }

    std::unordered_map<std::uint64_t, MetricService::NetworkState::InterfaceCounters> current;
    std::vector<NetworkInterfaceInfo> discovered;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const auto& row = table->Table[i];
        const bool connected = IsConnectedCandidate(row);
        const bool physical = IsPhysicalInterface(row);
        const bool selected = selectedLuid != 0 && row.InterfaceLuid.Value == selectedLuid;
        const bool included = connected && (selected || (selectedLuid == 0 &&
                                  (physical || includeVirtual)));
        if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
            discovered.push_back({row.InterfaceLuid.Value, FriendlyName(row), row.Description,
                                  connected, physical, included});
        }
        if (included) {
            current.emplace(row.InterfaceLuid.Value,
                            MetricService::NetworkState::InterfaceCounters{row.InOctets,
                                                                           row.OutOctets});
        }
    }
    FreeMibTable(table);

    inventory = std::move(discovered);
    const auto now = GetTickCount64();
    const auto elapsed = SaturatingDelta(now, state.timestamp);
    const bool reset = !state.initialized || selectionGeneration != appliedGeneration ||
                       elapsed == 0 || elapsed > static_cast<std::uint64_t>(intervalMs) * 3ULL;
    if (!reset && !current.empty()) {
        for (const auto& [interfaceId, counters] : current) {
            const auto previous = state.interfaces.find(interfaceId);
            if (previous == state.interfaces.end()) {
                continue;
            }
            snapshot.networkDownloadBytesPerSecond +=
                PreciseBytesPerSecond(counters.receive, previous->second.receive, elapsed);
            snapshot.networkUploadBytesPerSecond +=
                PreciseBytesPerSecond(counters.transmit, previous->second.transmit, elapsed);
        }
    }
    state.interfaces = std::move(current);
    state.timestamp = now;
    state.initialized = true;
    appliedGeneration = selectionGeneration;
    snapshot.networkAvailable = !state.interfaces.empty();
    snapshot.networkReady = snapshot.networkAvailable && !reset;
    return true;
}

}  // namespace

struct MetricService::GpuState {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER utilizationCounter = nullptr;
    PDH_HCOUNTER dedicatedMemoryCounter = nullptr;
    PDH_HCOUNTER sharedMemoryCounter = nullptr;
    std::unordered_map<std::uint64_t, std::uint64_t> memoryCapacityByLuid;
    std::vector<GpuAdapterInfo> adapters;
    bool firstCollection = true;
    bool initialized = false;
    int consecutiveFailures = 0;

    ~GpuState() {
        if (query != nullptr) {
            PdhCloseQuery(query);
        }
    }

    bool Initialize() {
        if (query != nullptr) {
            PdhCloseQuery(query);
            query = nullptr;
        }
        utilizationCounter = nullptr;
        dedicatedMemoryCounter = nullptr;
        sharedMemoryCounter = nullptr;
        firstCollection = true;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) {
            return false;
        }
        const bool hasUtilization =
            PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0,
                                  &utilizationCounter) == ERROR_SUCCESS;
        const bool hasDedicatedMemory =
            PdhAddEnglishCounterW(query, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0,
                                  &dedicatedMemoryCounter) == ERROR_SUCCESS;
        const bool hasSharedMemory =
            PdhAddEnglishCounterW(query, L"\\GPU Adapter Memory(*)\\Shared Usage", 0,
                                  &sharedMemoryCounter) == ERROR_SUCCESS;
        if (!hasUtilization && !hasDedicatedMemory && !hasSharedMemory) {
            PdhCloseQuery(query);
            query = nullptr;
            return false;
        }
        initialized = PdhCollectQueryData(query) == ERROR_SUCCESS;
        QueryMemoryCapacity();
        return initialized;
    }

    void QueryMemoryCapacity() {
        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            return;
        }
        memoryCapacityByLuid.clear();
        adapters.clear();
        for (UINT index = 0;; ++index) {
            IDXGIAdapter1* adapter = nullptr;
            const HRESULT status = factory->EnumAdapters1(index, &adapter);
            if (status == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(status) || adapter == nullptr) {
                continue;
            }
            DXGI_ADAPTER_DESC1 description{};
            if (SUCCEEDED(adapter->GetDesc1(&description)) &&
                !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                const std::uint64_t capacity = description.DedicatedVideoMemory >
                                                    std::numeric_limits<std::uint64_t>::max() - description.SharedSystemMemory
                                                ? std::numeric_limits<std::uint64_t>::max()
                                                : description.DedicatedVideoMemory + description.SharedSystemMemory;
                LUID luid = description.AdapterLuid;
                memoryCapacityByLuid[luid.LowPart | (static_cast<std::uint64_t>(luid.HighPart) << 32)] = capacity;
                adapters.push_back({luid.LowPart | (static_cast<std::uint64_t>(luid.HighPart) << 32),
                                    description.Description, capacity});
            }
            adapter->Release();
        }
        factory->Release();
    }

    static std::uint64_t ParseLuid(const wchar_t* instanceName) {
        if (instanceName == nullptr) return 0;
        const wchar_t* marker = wcsstr(instanceName, L"luid_0x");
        if (marker == nullptr) return 0;
        wchar_t* end = nullptr;
        const auto high = wcstoul(marker + 7, &end, 16);
        if (end == marker + 7 || wcsncmp(end, L"_0x", 3) != 0) return 0;
        const auto low = wcstoul(end + 3, &end, 16);
        return (static_cast<std::uint64_t>(high) << 32) | low;
    }

    static bool ReadCounter(PDH_HCOUNTER counter, bool maximum, double& value,
                            std::uint64_t selectedLuid) {
        if (counter == nullptr) {
            return false;
        }
        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        const auto status =
            PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
        if (status != PDH_MORE_DATA || bufferSize == 0) {
            return false;
        }

        std::vector<std::byte> buffer(bufferSize);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &itemCount,
                                         items) != ERROR_SUCCESS) {
            return false;
        }

        value = 0.0;
        bool found = false;
        for (DWORD i = 0; i < itemCount; ++i) {
            if (selectedLuid != 0 && ParseLuid(items[i].szName) != selectedLuid) continue;
            if (items[i].FmtValue.CStatus == ERROR_SUCCESS &&
                std::isfinite(items[i].FmtValue.doubleValue)) {
                const double sample = std::max(0.0, items[i].FmtValue.doubleValue);
                value = maximum ? std::max(value, sample) : value + sample;
                found = true;
            }
        }
        return found;
    }

    std::uint64_t MemoryCapacity(std::uint64_t selectedLuid) const {
        if (selectedLuid != 0) {
            const auto item = memoryCapacityByLuid.find(selectedLuid);
            return item == memoryCapacityByLuid.end() ? 0 : item->second;
        }
        std::uint64_t total = 0;
        for (const auto& [_, capacity] : memoryCapacityByLuid) {
            total = std::numeric_limits<std::uint64_t>::max() - total < capacity
                        ? std::numeric_limits<std::uint64_t>::max() : total + capacity;
        }
        return total;
    }

    bool Sample(double& utilization, std::uint64_t& memoryUsedBytes,
                std::uint64_t& memoryTotalBytes, bool& memoryAvailable,
                std::uint64_t selectedLuid) {
        utilization = 0.0;
        memoryUsedBytes = 0;
        memoryTotalBytes = MemoryCapacity(selectedLuid);
        memoryAvailable = false;
        if (query == nullptr || PdhCollectQueryData(query) != ERROR_SUCCESS) {
            ++consecutiveFailures;
            if (consecutiveFailures >= 2) Initialize();
            return false;
        }
        consecutiveFailures = 0;
        if (firstCollection) {
            firstCollection = false;
            return false;
        }

        double maximumUtilization = 0.0;
        const bool hasUtilization = ReadCounter(utilizationCounter, true, maximumUtilization, selectedLuid);
        utilization = std::clamp(maximumUtilization, 0.0, 100.0);

        // DXGI QueryVideoMemoryInfo reports the calling process's budget and usage, not
        // a GPU-wide total. The Adapter Memory counters provide the system-wide values.
        double dedicatedBytes = 0.0;
        double sharedBytes = 0.0;
        const bool hasDedicated = ReadCounter(dedicatedMemoryCounter, false, dedicatedBytes, selectedLuid);
        const bool hasShared = ReadCounter(sharedMemoryCounter, false, sharedBytes, selectedLuid);
        memoryAvailable = hasDedicated || hasShared;
        if (memoryAvailable) {
            const long double total = std::max(0.0, dedicatedBytes) + std::max(0.0, sharedBytes);
            memoryUsedBytes = total >= static_cast<long double>(std::numeric_limits<std::uint64_t>::max())
                                  ? std::numeric_limits<std::uint64_t>::max()
                                  : static_cast<std::uint64_t>(total);
        }
        return hasUtilization || hasDedicated || hasShared;
    }
};

MetricService::MetricService() = default;

MetricService::~MetricService() {
    Stop();
}

bool MetricService::Start(int intervalMs, HWND notificationWindow) {
    if (running_.exchange(true)) {
        return false;
    }
    intervalMs_.store(intervalMs == 500 || intervalMs == 2000 ? intervalMs : 1000);
    notificationWindow_ = notificationWindow;
    gpu_ = std::make_unique<GpuState>();
    gpu_->Initialize();
    worker_ = std::thread(&MetricService::Run, this);
    return true;
}

void MetricService::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    wakeCondition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    gpu_.reset();
}

void MetricService::SetInterval(int intervalMs) {
    intervalMs_.store(intervalMs == 500 || intervalMs == 2000 ? intervalMs : 1000);
    wakeCondition_.notify_all();
}

void MetricService::SetNetworkSelection(std::uint64_t luid, bool includeVirtualInterfaces) {
    const bool changed = selectedNetworkLuid_.exchange(luid) != luid ||
                         includeVirtualNetworkInterfaces_.exchange(includeVirtualInterfaces) !=
                             includeVirtualInterfaces;
    if (changed) networkSelectionGeneration_.fetch_add(1);
    wakeCondition_.notify_all();
}

void MetricService::SetGpuSelection(std::uint64_t luid) {
    selectedGpuLuid_.store(luid);
    wakeCondition_.notify_all();
}

std::shared_ptr<const MetricSnapshot> MetricService::Snapshot() const {
    return snapshot_.load(std::memory_order_acquire);
}

std::vector<NetworkInterfaceInfo> MetricService::NetworkInterfaces() const {
    std::scoped_lock lock(devicesMutex_);
    return networkInterfaces_;
}

std::vector<GpuAdapterInfo> MetricService::GpuAdapters() const {
    std::scoped_lock lock(devicesMutex_);
    return gpuAdapters_;
}

void MetricService::Run() {
    while (running_) {
        MetricSnapshot next;
        Sample(next);
        snapshot_.store(std::make_shared<MetricSnapshot>(next), std::memory_order_release);
        if (notificationWindow_ != nullptr) {
            PostMessageW(notificationWindow_, kMetricsReadyMessage, 0, 0);
        }

        std::unique_lock lock(wakeMutex_);
        wakeCondition_.wait_for(lock, std::chrono::milliseconds(intervalMs_.load()),
                                [this] { return !running_.load(); });
    }
}

void MetricService::Sample(MetricSnapshot& snapshot) {
    snapshot.timestampMs = GetTickCount64();
    ReadSystemCpu(cpu_, snapshot.cpuPercent);
    ReadMemory(snapshot);
    std::vector<NetworkInterfaceInfo> interfaces;
    ReadNetwork(network_, snapshot, selectedNetworkLuid_.load(),
                includeVirtualNetworkInterfaces_.load(), networkSelectionGeneration_.load(),
                appliedNetworkSelectionGeneration_, intervalMs_.load(), interfaces);

    if (gpu_ != nullptr) {
        snapshot.gpuAvailable = gpu_->Sample(snapshot.gpuUtilPercent, snapshot.gpuMemoryUsedBytes,
                                             snapshot.gpuMemoryTotalBytes,
                                             snapshot.gpuMemoryAvailable,
                                             selectedGpuLuid_.load());
        std::scoped_lock lock(devicesMutex_);
        networkInterfaces_ = std::move(interfaces);
        gpuAdapters_ = gpu_->adapters;
    }
}

}  // namespace sysglance
