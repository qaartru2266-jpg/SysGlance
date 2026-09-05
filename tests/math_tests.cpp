#include "common.h"

#include <cassert>
#include <cmath>

using namespace sysglance;

int main() {
    const AppConfig defaults{};
    assert(!defaults.showGpu);
    assert(!defaults.showPercentDecimal);
    assert(!defaults.showNetworkArrows);

    const std::vector<NetworkInterfaceInfo> connectedNetwork{{1, L"Ethernet", L"", true, true, true}};
    const std::vector<NetworkInterfaceInfo> disconnectedNetwork{{1, L"Ethernet", L"", false, true, false}};
    assert(!ShouldFallbackToAggregateNetwork(0, connectedNetwork));
    assert(!ShouldFallbackToAggregateNetwork(1, connectedNetwork));
    assert(ShouldFallbackToAggregateNetwork(1, disconnectedNetwork));
    assert(ShouldFallbackToAggregateNetwork(2, connectedNetwork));
    assert(!ShouldFallbackToAggregateNetwork(1, {}));

    assert(HasSameNetworkMembers({1, 2}, {2, 1}));
    assert(!HasSameNetworkMembers({1}, {1, 2}));
    assert(!HasSameNetworkMembers({1, 2}, {1, 3}));
    assert(HasSameNetworkMembers({}, {}));

    assert(SaturatingDelta(100, 40) == 60);
    assert(SaturatingDelta(40, 100) == 0);
    assert(BytesPerSecond(2000, 1000, 1000) == 1000);
    assert(BytesPerSecond(2000, 1000, 500) == 2000);
    assert(BytesPerSecond(2000, 1000, 0) == 0);
    assert(std::abs(PreciseBytesPerSecond(2000, 1000, 400) - 2500.0) < 0.001);
    assert(PreciseBytesPerSecond(2000, 1000, 0) == 0.0);
    assert(IsKnownRefreshInterval(500));
    assert(IsKnownRefreshInterval(1000));
    assert(IsKnownRefreshInterval(2000));
    assert(!IsKnownRefreshInterval(750));

    assert(FormatNetworkRate(0.0) == L" 0.0K");
    assert(FormatNetworkRate(100.0) == L" 0.1K");
    assert(FormatNetworkRate(50.0 * 1024.0) == L"50.0K");
    assert(FormatNetworkRate(99.0 * 1024.0) == L"99.0K");
    // 100 KiB/s ~= 0.0977 MiB/s -> one-decimal 0.1M
    assert(FormatNetworkRate(100.0 * 1024.0) == L" 0.1M");
    assert(FormatNetworkRate(1024.0 * 1024.0) == L" 1.0M");
    assert(FormatNetworkRate(9.9 * 1024.0) == L" 9.9K");
    assert(FormatNetworkRate(10.0 * 1024.0) == L"10.0K");
    assert(FormatNetworkRate(99.9 * 1024.0) == L"99.9K");
    assert(FormatNetworkRate(0.0).size() == kHudNetworkRateSlotWidth);
    assert(FormatNetworkRate(10.0 * 1024.0).size() == kHudNetworkRateSlotWidth);
    assert(FormatNetworkRate(100.0 * 1024.0).size() == kHudNetworkRateSlotWidth);
    assert(FormatUnavailableNetworkRate().size() == kHudNetworkRateSlotWidth);
    return 0;
}
