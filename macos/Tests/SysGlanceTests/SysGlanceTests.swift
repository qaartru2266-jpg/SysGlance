import XCTest
@testable import SysGlance

final class SysGlanceTests: XCTestCase {
    func testNetworkUsesKBelow100KiB() {
        let rate = MetricFormatter.networkRate(99 * 1_024)
        XCTAssertEqual(rate.value, "99.0")
        XCTAssertEqual(rate.unit, "K")
    }

    func testNetworkUsesMAt100KiB() {
        let rate = MetricFormatter.networkRate(100 * 1_024)
        XCTAssertEqual(rate.value, " 0.1")
        XCTAssertEqual(rate.unit, "M")
    }

    func testUnavailableNetworkHasNoRawBytesFallback() {
        let rate = MetricFormatter.networkRate(nil)
        XCTAssertEqual(rate.value, " N/A")
        XCTAssertEqual(rate.unit, " ")
    }

    func testConfigurationNormalizesMutualExclusionAndRanges() {
        var config = AppConfig.recommended
        config.refreshMilliseconds = 123
        config.hudWidth = 2
        config.contentOpacity = 9
        config.mouseThrough = true
        config.locked = false
        config.normalize()
        XCTAssertEqual(config.refreshMilliseconds, 1_000)
        XCTAssertEqual(config.hudWidth, 180)
        XCTAssertEqual(config.contentOpacity, 1)
        XCTAssertTrue(config.locked)
    }

    func testHudKeepsNetworkWhenGPUIsUnavailable() {
        var config = AppConfig.recommended
        config.showGPU = true
        config.showNetworkArrows = true
        let snapshot = MetricSnapshot(
            sampledAt: .now, cpuPercent: 12.4, memoryUsedGiB: 8.0, memoryPercent: 50,
            gpuUsedGiB: nil, gpuPercent: nil, downloadBytesPerSecond: 102_400, uploadBytesPerSecond: 0
        )
        let text = MetricFormatter.hudText(snapshot: snapshot, config: config)
        XCTAssertTrue(text.contains("G N/A"))
        XCTAssertTrue(text.contains("↓"))
        XCTAssertTrue(text.contains("M"))
    }

    func testMenuBarRespectsDisabledMetrics() {
        var config = AppConfig.recommended
        config.showCPU = false
        config.showMemory = false
        config.showNetwork = false
        XCTAssertEqual(MetricFormatter.menuBarText(snapshot: .unavailable, config: config), "SysGlance")
    }

    func testHudCPUAndMemoryUseFixedTwoDigitIntegerSlots() {
        var config = AppConfig.recommended
        config.memoryDisplayMode = .percentage
        let snapshot = MetricSnapshot(
            sampledAt: .now, cpuPercent: 5.1, memoryUsedGiB: 8, memoryPercent: 42.4,
            gpuUsedGiB: nil, gpuPercent: nil, downloadBytesPerSecond: nil, uploadBytesPerSecond: nil
        )
        let text = MetricFormatter.hudText(snapshot: snapshot, config: config)
        XCTAssertTrue(text.hasPrefix("C05 42"))
    }

    func testRecommendedConfigurationMatchesWindowsV101Defaults() {
        let config = AppConfig.recommended
        XCTAssertEqual(config.displayMode, .menuBar)
        XCTAssertFalse(config.showGPU)
        XCTAssertEqual(config.memoryDisplayMode, .usedGiB)
        XCTAssertEqual(config.percentPrecision, .integer)
        XCTAssertFalse(config.showNetworkArrows)
        XCTAssertEqual(config.refreshMilliseconds, 1_000)
    }

    func testExistingConfigurationPreservesExplicitPreferencesAfterDefaultChange() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let url = directory.appendingPathComponent("config.ini")
        defer { try? FileManager.default.removeItem(at: directory) }
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try """
        [SysGlance]
        displayMode=hud
        showGPU=false
        memoryDisplayMode=percentage
        percentPrecision=integer
        showNetworkArrows=false
        hudWidth=180
        hudHeight=24
        fontSize=8
        """.write(to: url, atomically: true, encoding: .utf8)

        let loaded = ConfigService(configURL: url).load().config
        XCTAssertEqual(loaded.displayMode, .hud)
        XCTAssertFalse(loaded.showGPU)
        XCTAssertEqual(loaded.memoryDisplayMode, .percentage)
        XCTAssertEqual(loaded.percentPrecision, .integer)
        XCTAssertFalse(loaded.showNetworkArrows)
        XCTAssertEqual(loaded.hudWidth, 180)
        XCTAssertEqual(loaded.hudHeight, 24)
        XCTAssertEqual(loaded.fontSize, 8)
    }

    func testNetworkFieldsRemainFiveCharactersAtLiveAndUnavailableStates() {
        let available = MetricFormatter.networkRate(102_400)
        let unavailable = MetricFormatter.networkRate(nil)
        XCTAssertEqual((available.value + available.unit).count, 5)
        XCTAssertEqual((unavailable.value + unavailable.unit).count, 5)
    }

    func testSummaryDistinguishesNetworkInitializationFromUnavailableNetwork() {
        var initializing = MetricSnapshot.unavailable
        initializing = MetricSnapshot(
            sampledAt: .now, cpuPercent: nil, memoryUsedGiB: nil, memoryPercent: nil,
            gpuUsedGiB: nil, gpuPercent: nil, downloadBytesPerSecond: nil, uploadBytesPerSecond: nil,
            networkState: .initializing
        )
        XCTAssertTrue(MetricFormatter.summary(snapshot: initializing, config: .recommended).contains("Network initializing"))
        XCTAssertTrue(MetricFormatter.summary(snapshot: .unavailable, config: .recommended).contains("Network N/A"))
    }
}
