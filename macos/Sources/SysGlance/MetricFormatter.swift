import Foundation

enum MetricFormatter {
    private static let posix = Locale(identifier: "en_US_POSIX")

    static func hudText(snapshot: MetricSnapshot, config: AppConfig) -> String {
        var slots: [String] = []
        if config.showCPU { slots.append("C" + hudPercent(snapshot.cpuPercent)) }
        if config.showMemory {
            let value = config.memoryDisplayMode == .usedGiB ? snapshot.memoryUsedGiB : snapshot.memoryPercent
            slots.append(config.memoryDisplayMode == .percentage ? hudPercent(value) : fixedGiB(value))
        }
        if config.showGPU {
            guard let used = snapshot.gpuUsedGiB, let percent = snapshot.gpuPercent else {
                slots.append("G N/A")
                return joined(slots, network: config.showNetwork ? network(snapshot, arrows: config.showNetworkArrows) : nil)
            }
            slots.append("G\(number(used, digits: 1))/\(number(percent, digits: config.percentPrecision.digits))")
        }
        return joined(slots, network: config.showNetwork ? network(snapshot, arrows: config.showNetworkArrows) : nil)
    }

    static func menuBarText(snapshot: MetricSnapshot, config: AppConfig) -> String {
        var slots: [String] = []
        if config.showCPU { slots.append("C" + percent(snapshot.cpuPercent, precision: config.percentPrecision)) }
        if config.showMemory {
            let memoryValue = config.memoryDisplayMode == .usedGiB ? snapshot.memoryUsedGiB : snapshot.memoryPercent
            slots.append("M" + percent(memoryValue, precision: config.memoryDisplayMode == .usedGiB ? .oneDecimal : config.percentPrecision))
        }
        if config.showNetwork {
            let down = shortNetwork(snapshot.downloadBytesPerSecond)
            let up = shortNetwork(snapshot.uploadBytesPerSecond)
            slots.append(config.showNetworkArrows ? "↓\(down) ↑\(up)" : "\(down) \(up)")
        }
        return slots.isEmpty ? "SysGlance" : slots.joined(separator: "  ")
    }

    static func summary(snapshot: MetricSnapshot, config: AppConfig) -> String {
        let cpu = "CPU \(percent(snapshot.cpuPercent, precision: config.percentPrecision))%"
        let memory = "Memory \(number(snapshot.memoryUsedGiB, digits: 1)) GiB (\(percent(snapshot.memoryPercent, precision: config.percentPrecision))%)"
        let gpu: String
        if let used = snapshot.gpuUsedGiB, let percent = snapshot.gpuPercent {
            gpu = "GPU \(number(used, digits: 1)) GiB (\(number(percent, digits: config.percentPrecision.digits))%)"
        } else { gpu = "GPU N/A" }
        let network: String
        switch snapshot.networkState {
        case .ready:
            network = "Down \(fullNetwork(snapshot.downloadBytesPerSecond)), Up \(fullNetwork(snapshot.uploadBytesPerSecond))"
        case .initializing:
            network = "Network initializing"
        case .unavailable:
            network = "Network N/A"
        }
        return "\(cpu) | \(memory) | \(gpu) | \(network)"
    }

    static func networkRate(_ bytesPerSecond: Double?) -> (value: String, unit: String) {
        // Four numeric characters plus one magnitude character keeps N/A and live rates aligned.
        guard let bytesPerSecond, bytesPerSecond >= 0 else { return (" N/A", " ") }
        let kib = bytesPerSecond / 1_024
        if kib < 100 { return (fixedNetworkNumber(kib), "K") }
        return (fixedNetworkNumber(kib / 1_024), "M")
    }

    private static func joined(_ slots: [String], network: String?) -> String {
        return (slots + (network.map { [$0] } ?? [])).joined(separator: " ")
    }

    private static func network(_ snapshot: MetricSnapshot, arrows: Bool) -> String {
        let down = networkRate(snapshot.downloadBytesPerSecond)
        let up = networkRate(snapshot.uploadBytesPerSecond)
        let downField = down.value + down.unit
        let upField = up.value + up.unit
        if arrows { return "↓ \(downField) ↑ \(upField)" }
        return "\(downField) \(upField)"
    }

    private static func shortNetwork(_ bytes: Double?) -> String {
        guard let bytes, bytes >= 0 else { return "N/A" }
        let kib = bytes / 1_024
        return kib < 100 ? number(kib, digits: 1) + "K" : number(kib / 1_024, digits: 1) + "M"
    }

    private static func fullNetwork(_ bytes: Double?) -> String {
        let formatted = networkRate(bytes)
        return formatted.value + formatted.unit + "/s"
    }

    private static func percent(_ value: Double?, precision: PercentPrecision) -> String { number(value, digits: precision.digits) }

    /// HUD percentages always occupy two characters: 00...99. A saturated 100% is displayed as 99
    /// so live values never shift the following slots; the expanded menu summary still reports 100%.
    private static func hudPercent(_ value: Double?) -> String {
        guard let value, value.isFinite else { return "N/A" }
        return String(format: "%02d", locale: posix, min(99, max(0, Int(value.rounded()))))
    }

    private static func fixedGiB(_ value: Double?) -> String {
        guard let value, value.isFinite else { return "N/A" }
        return String(format: "%04.1f", locale: posix, value)
    }

    /// The HUD reserves four characters for each rate (" 0.1" through "999+").
    /// The magnitude suffix makes every available network field exactly five characters wide.
    private static func fixedNetworkNumber(_ value: Double) -> String {
        if value >= 999.95 { return "999+" }
        return String(format: "%4.1f", locale: posix, max(0, value))
    }


    private static func number(_ value: Double?, digits: Int, width: Int? = nil) -> String {
        guard let value, value.isFinite else { return width.map { String(format: "%\($0)s", "N/A") } ?? "N/A" }
        let format = width.map { "%\($0).\(digits)f" } ?? "%.\(digits)f"
        let plain = String(format: format, locale: posix, value)
        return plain.count > 10 ? String(format: "%.2e", locale: posix, value) : plain
    }
}
