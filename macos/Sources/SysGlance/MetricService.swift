import Darwin
import Foundation

final class MetricService: @unchecked Sendable {
    var onSnapshot: (@Sendable (MetricSnapshot) -> Void)?

    private let queue = DispatchQueue(label: "com.sysglance.metrics", qos: .utility)
    private var timer: DispatchSourceTimer?
    private var configuration: AppConfig
    private var previousCPUTicks: [UInt32]?
    private var previousNetwork: (received: UInt64, sent: UInt64, interfaces: Set<String>, date: Date)?

    init(configuration: AppConfig) { self.configuration = configuration }

    func start() { schedule() }

    func update(configuration: AppConfig) {
        queue.async { [weak self] in
            guard let self else { return }
            let intervalChanged = self.configuration.refreshMilliseconds != configuration.refreshMilliseconds
            self.configuration = configuration
            if intervalChanged { self.schedule() }
        }
    }

    func stop() {
        queue.sync { timer?.cancel(); timer = nil }
    }

    /// Drops adjacent-sample state after sleep/wake or a display/network transition.
    /// The next CPU and network samples intentionally report N/A instead of a spike.
    func resetBaselines() {
        queue.async { [weak self] in
            self?.previousCPUTicks = nil
            self?.previousNetwork = nil
        }
    }

    private func schedule() {
        timer?.cancel()
        let newTimer = DispatchSource.makeTimerSource(queue: queue)
        let interval = DispatchTimeInterval.milliseconds(configuration.refreshMilliseconds)
        newTimer.schedule(deadline: .now(), repeating: interval, leeway: .milliseconds(80))
        newTimer.setEventHandler { [weak self] in self?.sample() }
        timer = newTimer
        newTimer.resume()
    }

    private func sample() {
        let memory = memoryStats()
        let network = networkStats()
        let snapshot = MetricSnapshot(
            sampledAt: .now,
            cpuPercent: cpuPercent(),
            memoryUsedGiB: memory.usedGiB,
            memoryPercent: memory.percent,
            gpuUsedGiB: nil,
            gpuPercent: nil,
            downloadBytesPerSecond: network.down,
            uploadBytesPerSecond: network.up,
            networkState: network.state
        )
        if let onSnapshot {
            DispatchQueue.main.async { onSnapshot(snapshot) }
        }
    }

    private func cpuPercent() -> Double? {
        var processorCount: natural_t = 0
        var info: processor_info_array_t?
        var infoCount: mach_msg_type_number_t = 0
        let result = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processorCount, &info, &infoCount)
        guard result == KERN_SUCCESS, let info else { return nil }
        defer { vm_deallocate(mach_task_self_, vm_address_t(UInt(bitPattern: info)), vm_size_t(infoCount) * vm_size_t(MemoryLayout<integer_t>.stride)) }
        let count = Int(processorCount) * Int(CPU_STATE_MAX)
        let ticks = Array(UnsafeBufferPointer(start: info, count: count)).map(UInt32.init)
        defer { previousCPUTicks = ticks }
        guard let previous = previousCPUTicks, previous.count == ticks.count else { return nil }
        var busy: UInt64 = 0
        var total: UInt64 = 0
        for offset in stride(from: 0, to: ticks.count, by: Int(CPU_STATE_MAX)) {
            for state in 0..<Int(CPU_STATE_MAX) {
                let delta = UInt64(ticks[offset + state] &- previous[offset + state])
                total += delta
                if state != Int(CPU_STATE_IDLE) { busy += delta }
            }
        }
        guard total > 0 else { return nil }
        return min(100, max(0, Double(busy) * 100 / Double(total)))
    }

    private func memoryStats() -> (usedGiB: Double?, percent: Double?) {
        var vm = vm_statistics64()
        var count = mach_msg_type_number_t(MemoryLayout<vm_statistics64_data_t>.stride / MemoryLayout<integer_t>.stride)
        let result = withUnsafeMutablePointer(to: &vm) {
            $0.withMemoryRebound(to: integer_t.self, capacity: Int(count)) {
                host_statistics64(mach_host_self(), HOST_VM_INFO64, $0, &count)
            }
        }
        guard result == KERN_SUCCESS else { return (nil, nil) }
        let pageSize = UInt64(getpagesize())
        // Matches the user-facing "Memory Used" concept: active + wired + compressed pages.
        let usedPages = UInt64(vm.active_count) + UInt64(vm.wire_count) + UInt64(vm.compressor_page_count)
        let usedBytes = usedPages * pageSize
        let totalBytes = ProcessInfo.processInfo.physicalMemory
        guard totalBytes > 0 else { return (nil, nil) }
        return (Double(usedBytes) / 1_073_741_824, Double(usedBytes) * 100 / Double(totalBytes))
    }

    private func networkStats() -> (down: Double?, up: Double?, state: NetworkState) {
        guard let totals = interfaceTotals() else { return (nil, nil, .unavailable) }
        let now = Date()
        defer { previousNetwork = (totals.received, totals.sent, totals.interfaces, now) }
        guard let previous = previousNetwork else { return (nil, nil, .initializing) }
        let elapsed = now.timeIntervalSince(previous.date)
        let expected = Double(configuration.refreshMilliseconds) / 1_000
        guard elapsed > 0, elapsed <= expected * 3,
              previous.interfaces == totals.interfaces,
              totals.received >= previous.received, totals.sent >= previous.sent else { return (nil, nil, .initializing) }
        return (Double(totals.received - previous.received) / elapsed, Double(totals.sent - previous.sent) / elapsed, .ready)
    }

    private func interfaceTotals() -> (received: UInt64, sent: UInt64, interfaces: Set<String>)? {
        var head: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&head) == 0, let first = head else { return nil }
        defer { freeifaddrs(head) }
        var received: UInt64 = 0
        var sent: UInt64 = 0
        var interfaces = Set<String>()
        var current: UnsafeMutablePointer<ifaddrs>? = first
        while let item = current {
            defer { current = item.pointee.ifa_next }
            let flags = Int32(item.pointee.ifa_flags)
            guard (flags & IFF_UP) != 0, (flags & IFF_LOOPBACK) == 0,
                  item.pointee.ifa_addr?.pointee.sa_family == UInt8(AF_LINK),
                  let namePointer = item.pointee.ifa_name else { continue }
            let name = String(cString: namePointer)
            // macOS physical Wi-Fi/Ethernet adapters use the en* namespace. Virtual/VPN adapters are excluded.
            guard name.hasPrefix("en"), let rawData = item.pointee.ifa_data else { continue }
            let data = rawData.assumingMemoryBound(to: if_data.self).pointee
            received += UInt64(data.ifi_ibytes)
            sent += UInt64(data.ifi_obytes)
            interfaces.insert(name)
        }
        return interfaces.isEmpty ? nil : (received, sent, interfaces)
    }
}
