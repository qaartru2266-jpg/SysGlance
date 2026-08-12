#include "common.h"

#include <cassert>
#include <cmath>

using namespace sysglance;

int main() {
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

    assert(FormatNetworkRate(0.0) == L"   0.0KB ");
    assert(FormatNetworkRate(100.0) == L"   0.1KB ");
    assert(FormatNetworkRate(50.0 * 1024.0) == L"  50.0KB ");
    assert(FormatNetworkRate(99.0 * 1024.0) == L"  99.0KB ");
    // 100 KiB/s ~= 0.0977 MiB/s -> one-decimal 0.1MB
    assert(FormatNetworkRate(100.0 * 1024.0) == L"   0.1MB ");
    assert(FormatNetworkRate(1024.0 * 1024.0) == L"   1.0MB ");
    assert(FormatNetworkRate(0.0).size() == 9);
    assert(FormatNetworkRate(100.0 * 1024.0).size() == 9);
    return 0;
}
