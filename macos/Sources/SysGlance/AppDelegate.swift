import AppKit

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let configService = ConfigService()
    private var config = AppConfig.recommended
    private var lastGood: AppConfig?
    private var snapshot = MetricSnapshot.unavailable
    private var metricService: MetricService!
    private var hud: HudPanel!
    private var statusItem: NSStatusItem!
    private var settingsController: SettingsWindowController?
    private let summaryItem = NSMenuItem(title: "正在采样…", action: nil, keyEquivalent: "")
    private var notificationTokens: [NSObjectProtocol] = []

    func applicationDidFinishLaunching(_ notification: Notification) {
        let loaded = configService.load()
        config = loaded.config; lastGood = loaded.lastGood
        createStatusItem()
        hud = HudPanel(config: config)
        hud.onPositionChanged = { [weak self] origin in
            self?.config.hudX = origin.x; self?.config.hudY = origin.y
        }
        updateVisibility()
        metricService = MetricService(configuration: config)
        metricService.onSnapshot = { [weak self] snapshot in
            Task { @MainActor [weak self] in self?.consume(snapshot) }
        }
        metricService.start()
        observeSystemChanges()
    }

    func applicationWillTerminate(_ notification: Notification) {
        metricService?.stop()
        notificationTokens.forEach(NotificationCenter.default.removeObserver)
        persist()
    }

    private func createStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        guard let button = statusItem.button else { return }
        button.image = NSImage(systemSymbolName: "chart.bar.fill", accessibilityDescription: "SysGlance")
        button.imagePosition = .imageLeading
        button.font = NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)
        let menu = NSMenu()
        summaryItem.isEnabled = false
        menu.addItem(summaryItem)
        menu.addItem(.separator())
        menu.addItem(withTitle: "显示桌面 HUD", action: #selector(showHud(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "仅菜单栏摘要", action: #selector(showMenuBar(_:)), keyEquivalent: "")
        menu.addItem(.separator())
        menu.addItem(withTitle: "打开设置…", action: #selector(openSettings(_:)), keyEquivalent: ",")
        menu.addItem(.separator())
        menu.addItem(withTitle: "退出 SysGlance", action: #selector(quit(_:)), keyEquivalent: "q")
        menu.items.forEach { $0.target = self }
        statusItem.menu = menu
        updateMenuState()
    }

    private func consume(_ newSnapshot: MetricSnapshot) {
        snapshot = newSnapshot
        hud.update(snapshot: snapshot, config: config)
        // Keep the menu bar footprint minimal; current values remain available in the menu summary.
        statusItem.button?.title = ""
        summaryItem.title = MetricFormatter.summary(snapshot: snapshot, config: config)
    }

    @objc private func showHud(_ sender: Any?) { config.displayMode = .hud; apply(config) }
    @objc private func showMenuBar(_ sender: Any?) { config.displayMode = .menuBar; apply(config) }

    @objc private func openSettings(_ sender: Any?) {
        let controller = SettingsWindowController(config: config, lastGood: lastGood) { [weak self] updated in
            self?.apply(updated) ?? false
        }
        settingsController = controller
        controller.showWindow(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc private func quit(_ sender: Any?) { NSApp.terminate(nil) }

    @discardableResult
    private func apply(_ newConfig: AppConfig) -> Bool {
        var normalized = newConfig
        normalized.normalize()
        let shouldRestoreRecommendedPosition = normalized.hudX == nil || normalized.hudY == nil
        config = normalized
        if config.displayMode == .hud && !shouldRestoreRecommendedPosition {
            let origin = hud.currentOrigin()
            config.hudX = origin.x; config.hudY = origin.y
        }
        hud.apply(config: config, reposition: shouldRestoreRecommendedPosition)
        let origin = hud.currentOrigin()
        config.hudX = origin.x; config.hudY = origin.y
        metricService.update(configuration: config)
        updateVisibility()
        updateMenuState()
        consume(snapshot)
        let candidateLastGood = config.displayMode == .hud ? config : (lastGood ?? config)
        let saved = persist(lastGood: candidateLastGood)
        if saved && config.displayMode == .hud { lastGood = config }
        return saved
    }

    private func updateVisibility() {
        if config.displayMode == .hud { hud.orderFrontRegardless() } else { hud.orderOut(nil) }
    }

    private func updateMenuState() {
        guard let menu = statusItem?.menu else { return }
        menu.item(withTitle: "显示桌面 HUD")?.state = config.displayMode == .hud ? .on : .off
        menu.item(withTitle: "仅菜单栏摘要")?.state = config.displayMode == .menuBar ? .on : .off
    }

    private func observeSystemChanges() {
        let workspace = NSWorkspace.shared.notificationCenter
        notificationTokens.append(workspace.addObserver(forName: NSWorkspace.didWakeNotification, object: nil, queue: .main) { [weak self] _ in
            Task { @MainActor [weak self] in self?.metricService.resetBaselines() }
        })
        notificationTokens.append(NotificationCenter.default.addObserver(forName: NSApplication.didChangeScreenParametersNotification, object: nil, queue: .main) { [weak self] _ in
            Task { @MainActor [weak self] in
                self?.metricService.resetBaselines()
                self?.reclampHUD()
            }
        })
    }

    private func reclampHUD() {
        guard hud != nil else { return }
        hud.apply(config: config, reposition: false)
        let origin = hud.currentOrigin()
        config.hudX = origin.x
        config.hudY = origin.y
        persist()
    }

    @discardableResult
    private func persist(lastGood: AppConfig? = nil) -> Bool {
        if hud != nil {
            let origin = hud.currentOrigin()
            config.hudX = origin.x; config.hudY = origin.y
        }
        do {
            try configService.save(config: config, lastGood: lastGood ?? self.lastGood ?? config)
            return true
        } catch {
            NSLog("SysGlance could not save config: %@", error.localizedDescription)
            return false
        }
    }
}
