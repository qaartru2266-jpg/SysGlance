import AppKit
import Foundation

enum AppMetadata {
    static let version = "2.0.0"
}

enum DisplayMode: String, CaseIterable, Sendable {
    case hud
    case menuBar
}

enum MemoryDisplayMode: String, CaseIterable, Sendable {
    case usedGiB
    case percentage
}

enum PercentPrecision: String, CaseIterable, Sendable {
    case oneDecimal
    case integer

    var digits: Int { self == .oneDecimal ? 1 : 0 }
}

struct MetricSnapshot: Sendable {
    let sampledAt: Date
    let cpuPercent: Double?
    let memoryUsedGiB: Double?
    let memoryPercent: Double?
    let gpuUsedGiB: Double?
    let gpuPercent: Double?
    let downloadBytesPerSecond: Double?
    let uploadBytesPerSecond: Double?
    let networkState: NetworkState

    init(
        sampledAt: Date,
        cpuPercent: Double?,
        memoryUsedGiB: Double?,
        memoryPercent: Double?,
        gpuUsedGiB: Double?,
        gpuPercent: Double?,
        downloadBytesPerSecond: Double?,
        uploadBytesPerSecond: Double?,
        networkState: NetworkState = .unavailable
    ) {
        self.sampledAt = sampledAt
        self.cpuPercent = cpuPercent
        self.memoryUsedGiB = memoryUsedGiB
        self.memoryPercent = memoryPercent
        self.gpuUsedGiB = gpuUsedGiB
        self.gpuPercent = gpuPercent
        self.downloadBytesPerSecond = downloadBytesPerSecond
        self.uploadBytesPerSecond = uploadBytesPerSecond
        self.networkState = networkState
    }

    static let unavailable = MetricSnapshot(
        sampledAt: .now, cpuPercent: nil, memoryUsedGiB: nil, memoryPercent: nil,
        gpuUsedGiB: nil, gpuPercent: nil, downloadBytesPerSecond: nil, uploadBytesPerSecond: nil,
        networkState: .unavailable
    )
}

enum NetworkState: Sendable {
    case unavailable
    case initializing
    case ready
}

struct AppConfig: Equatable, Sendable {
    // Windows v1.0.1's tray-first default maps to the macOS menu bar.
    var displayMode: DisplayMode = .menuBar
    var refreshMilliseconds: Int = 1_000
    var showCPU = true
    var showMemory = true
    var showGPU = false
    var showNetwork = true
    var memoryDisplayMode: MemoryDisplayMode = .usedGiB
    var percentPrecision: PercentPrecision = .integer
    var showNetworkArrows = false
    var hudWidth: CGFloat = 360
    var hudHeight: CGFloat = 34
    var fontSize: CGFloat = 12
    var borderWidth: CGFloat = 0.5
    var borderColorHex = "#FF9500"
    var textColorHex = "#F5F5F7"
    var backgroundColorHex = "#1C1C1E"
    var contentOpacity: CGFloat = 0.9
    var locked = false
    var mouseThrough = false
    var hudX: CGFloat?
    var hudY: CGFloat?

    static let recommended = AppConfig()

    mutating func normalize() {
        refreshMilliseconds = [500, 1_000, 2_000].contains(refreshMilliseconds) ? refreshMilliseconds : 1_000
        hudWidth = min(max(hudWidth, 180), 1_200)
        hudHeight = min(max(hudHeight, 24), 160)
        fontSize = min(max(fontSize, 8), 36)
        borderWidth = min(max(borderWidth, 0), 8)
        contentOpacity = min(max(contentOpacity, 0), 1)
        if mouseThrough { locked = true }
        borderColorHex = NSColor(hex: borderColorHex).hexString
        textColorHex = NSColor(hex: textColorHex).hexString
        backgroundColorHex = NSColor(hex: backgroundColorHex).hexString
    }
}

extension NSColor {
    convenience init(hex: String) {
        let stripped = hex.trimmingCharacters(in: .whitespacesAndNewlines).replacingOccurrences(of: "#", with: "")
        let value = UInt64(stripped, radix: 16) ?? 0
        let r = CGFloat((value >> 16) & 0xff) / 255
        let g = CGFloat((value >> 8) & 0xff) / 255
        let b = CGFloat(value & 0xff) / 255
        self.init(srgbRed: r, green: g, blue: b, alpha: 1)
    }

    var hexString: String {
        let converted = usingColorSpace(.sRGB) ?? self
        return String(format: "#%02X%02X%02X", Int((converted.redComponent * 255).rounded()), Int((converted.greenComponent * 255).rounded()), Int((converted.blueComponent * 255).rounded()))
    }
}
