import AppKit

final class SettingsWindowController: NSWindowController, NSWindowDelegate {
    private var draft: AppConfig
    private let lastGood: AppConfig?
    private let onApply: @MainActor (AppConfig) -> Bool

    private let mode = NSPopUpButton()
    private let refresh = NSPopUpButton()
    private let cpu = NSButton(checkboxWithTitle: "显示 CPU", target: nil, action: nil)
    private let memory = NSButton(checkboxWithTitle: "显示内存", target: nil, action: nil)
    private let gpu = NSButton(checkboxWithTitle: "显示 GPU", target: nil, action: nil)
    private let network = NSButton(checkboxWithTitle: "显示网络", target: nil, action: nil)
    private let memoryMode = NSPopUpButton()
    private let precision = NSPopUpButton()
    private let arrows = NSButton(checkboxWithTitle: "显示网络箭头", target: nil, action: nil)
    private let width = NSTextField()
    private let height = NSTextField()
    private let fontSize = NSTextField()
    private let opacity = NSSlider(value: 0.9, minValue: 0, maxValue: 1, target: nil, action: nil)
    private let borderColor = NSColorWell()
    private let textColor = NSColorWell()
    private let backgroundColor = NSColorWell()
    private let locked = NSButton(checkboxWithTitle: "锁定 HUD", target: nil, action: nil)
    private let mouseThrough = NSButton(checkboxWithTitle: "鼠标穿透（将同时锁定）", target: nil, action: nil)
    private let preview = SettingsPreview(frame: NSRect(x: 0, y: 0, width: 360, height: 52))

    init(config: AppConfig, lastGood: AppConfig?, onApply: @escaping @MainActor (AppConfig) -> Bool) {
        draft = config
        self.lastGood = lastGood
        self.onApply = onApply
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 560, height: 700), styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.title = "SysGlance 设置 · macOS v\(AppMetadata.version)"
        window.isReleasedWhenClosed = false
        super.init(window: window)
        window.delegate = self
        buildContent()
        populate(config)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    func windowWillClose(_ notification: Notification) { }

    @objc private func changed(_ sender: Any?) { preview.config = readDraft() }

    @objc private func apply(_ sender: Any?) {
        var updated = readDraft()
        updated.normalize()
        let saved = onApply(updated)
        let alert = NSAlert()
        alert.messageText = saved ? "设置已应用并保存" : "设置已应用，但配置未保存"
        alert.informativeText = saved
            ? "右键拖动 HUD；左键和双击均不会触发操作。"
            : "本次运行继续使用新设置，但重启后可能恢复旧设置。请检查“应用程序支持”目录的写入权限。"
        alert.addButton(withTitle: "好")
        alert.runModal()
        close()
    }

    @objc private func cancel(_ sender: Any?) { close() }

    @objc private func recommended(_ sender: Any?) {
        var defaultConfig = AppConfig.recommended
        defaultConfig.hudX = nil; defaultConfig.hudY = nil
        populate(defaultConfig)
    }

    @objc private func restoreLastGood(_ sender: Any?) {
        if let lastGood { populate(lastGood) }
    }

    @objc private func applyPreset(_ sender: NSPopUpButton) {
        switch sender.indexOfSelectedItem {
        case 1: borderColor.color = NSColor(hex: "#FF9500"); textColor.color = NSColor(hex: "#F5F5F7"); backgroundColor.color = NSColor(hex: "#1C1C1E")
        case 2: borderColor.color = NSColor(hex: "#0A84FF"); textColor.color = NSColor(hex: "#F5F9FF"); backgroundColor.color = NSColor(hex: "#101827")
        case 3: borderColor.color = NSColor(hex: "#8E8E93"); textColor.color = NSColor(hex: "#1D1D1F"); backgroundColor.color = NSColor(hex: "#F5F5F7")
        default: break
        }
        changed(sender)
    }

    private func buildContent() {
        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        let content = NSView()
        content.translatesAutoresizingMaskIntoConstraints = false
        scroll.documentView = content
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 22), stack.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -22),
            stack.topAnchor.constraint(equalTo: content.topAnchor, constant: 20), stack.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -20),
            content.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor)
        ])
        func label(_ text: String) -> NSTextField { let view = NSTextField(labelWithString: text); view.font = .systemFont(ofSize: 13, weight: .semibold); return view }
        func row(_ title: String, _ control: NSView) {
            let line = NSStackView(views: [NSTextField(labelWithString: title), control]); line.spacing = 12; line.alignment = .centerY
            line.views[0].widthAnchor.constraint(equalToConstant: 120).isActive = true
            stack.addArrangedSubview(line)
        }
        [mode, refresh, memoryMode, precision].forEach { $0.target = self; $0.action = #selector(changed(_:)) }
        [cpu, memory, gpu, network, arrows, locked, mouseThrough].forEach { $0.target = self; $0.action = #selector(changed(_:)) }
        [width, height, fontSize].forEach { $0.target = self; $0.action = #selector(changed(_:)); $0.widthAnchor.constraint(equalToConstant: 90).isActive = true }
        opacity.target = self; opacity.action = #selector(changed(_:)); opacity.widthAnchor.constraint(equalToConstant: 180).isActive = true
        [borderColor, textColor, backgroundColor].forEach { $0.target = self; $0.action = #selector(changed(_:)) }

        stack.addArrangedSubview(label("SysGlance macOS v\(AppMetadata.version)"))
        stack.addArrangedSubview(label("显示"))
        row("显示模式", mode); row("刷新间隔", refresh)
        let metrics = NSStackView(views: [cpu, memory, gpu, network]); metrics.spacing = 10; row("指标", metrics)
        row("内存显示", memoryMode); row("百分比精度", precision); row("网络", arrows)
        stack.addArrangedSubview(separator())
        stack.addArrangedSubview(label("HUD 外观"))
        let size = NSStackView(views: [width, NSTextField(labelWithString: "×"), height, NSTextField(labelWithString: "pt")]); size.spacing = 6; row("宽 × 高", size)
        row("字体大小", fontSize); row("内容透明度", opacity)
        row("边框颜色", borderColor); row("文字颜色", textColor); row("背景颜色", backgroundColor)
        let presets = NSPopUpButton(); presets.addItems(withTitles: ["自定义", "橙色深色", "蓝色深夜", "浅色玻璃"]); presets.target = self; presets.action = #selector(applyPreset(_:)); row("颜色预设", presets)
        preview.widthAnchor.constraint(equalToConstant: 500).isActive = true; preview.heightAnchor.constraint(equalToConstant: 52).isActive = true; stack.addArrangedSubview(preview)
        stack.addArrangedSubview(separator())
        stack.addArrangedSubview(label("行为与恢复"))
        stack.addArrangedSubview(locked); stack.addArrangedSubview(mouseThrough)
        let restore = NSStackView(); restore.spacing = 8
        let defaultButton = NSButton(title: "恢复推荐 HUD", target: self, action: #selector(recommended(_:)))
        let lastGoodButton = NSButton(title: "恢复上次可用布局", target: self, action: #selector(restoreLastGood(_:))); lastGoodButton.isEnabled = lastGood != nil
        restore.addArrangedSubview(defaultButton); restore.addArrangedSubview(lastGoodButton); stack.addArrangedSubview(restore)
        let buttons = NSStackView(); buttons.spacing = 8
        buttons.addArrangedSubview(NSButton(title: "取消", target: self, action: #selector(cancel(_:))))
        buttons.addArrangedSubview(NSButton(title: "应用", target: self, action: #selector(apply(_:))))
        stack.addArrangedSubview(buttons)
        window?.contentView = scroll
    }

    private func separator() -> NSBox { let box = NSBox(); box.boxType = .separator; box.widthAnchor.constraint(equalToConstant: 500).isActive = true; return box }

    private func populate(_ config: AppConfig) {
        draft = config
        mode.removeAllItems(); mode.addItems(withTitles: ["桌面 HUD", "菜单栏摘要"]); mode.selectItem(at: config.displayMode == .hud ? 0 : 1)
        refresh.removeAllItems(); refresh.addItems(withTitles: ["500 ms", "1000 ms", "2000 ms"]); refresh.selectItem(withTitle: "\(config.refreshMilliseconds) ms")
        cpu.state = config.showCPU ? .on : .off; memory.state = config.showMemory ? .on : .off; gpu.state = config.showGPU ? .on : .off; network.state = config.showNetwork ? .on : .off
        memoryMode.removeAllItems(); memoryMode.addItems(withTitles: ["已用 GiB", "使用率"]); memoryMode.selectItem(at: config.memoryDisplayMode == .usedGiB ? 0 : 1)
        precision.removeAllItems(); precision.addItems(withTitles: ["一位小数", "整数"]); precision.selectItem(at: config.percentPrecision == .oneDecimal ? 0 : 1)
        arrows.state = config.showNetworkArrows ? .on : .off
        width.stringValue = String(format: "%.0f", config.hudWidth); height.stringValue = String(format: "%.0f", config.hudHeight); fontSize.stringValue = String(format: "%.0f", config.fontSize); opacity.doubleValue = config.contentOpacity
        borderColor.color = NSColor(hex: config.borderColorHex); textColor.color = NSColor(hex: config.textColorHex); backgroundColor.color = NSColor(hex: config.backgroundColorHex)
        locked.state = config.locked ? .on : .off; mouseThrough.state = config.mouseThrough ? .on : .off
        preview.config = config
    }

    private func readDraft() -> AppConfig {
        var value = draft
        value.displayMode = mode.indexOfSelectedItem == 0 ? .hud : .menuBar
        value.refreshMilliseconds = Int(refresh.titleOfSelectedItem?.split(separator: " ").first ?? "1000") ?? 1_000
        value.showCPU = cpu.state == .on; value.showMemory = memory.state == .on; value.showGPU = gpu.state == .on; value.showNetwork = network.state == .on
        value.memoryDisplayMode = memoryMode.indexOfSelectedItem == 0 ? .usedGiB : .percentage
        value.percentPrecision = precision.indexOfSelectedItem == 0 ? .oneDecimal : .integer
        value.showNetworkArrows = arrows.state == .on
        value.hudWidth = CGFloat(Double(width.stringValue) ?? value.hudWidth); value.hudHeight = CGFloat(Double(height.stringValue) ?? value.hudHeight); value.fontSize = CGFloat(Double(fontSize.stringValue) ?? value.fontSize)
        value.contentOpacity = CGFloat(opacity.doubleValue)
        value.borderColorHex = borderColor.color.hexString; value.textColorHex = textColor.color.hexString; value.backgroundColorHex = backgroundColor.color.hexString
        value.locked = locked.state == .on; value.mouseThrough = mouseThrough.state == .on
        return value
    }
}

private final class SettingsPreview: NSView {
    var config = AppConfig.recommended { didSet { needsDisplay = true } }
    override func draw(_ dirtyRect: NSRect) {
        NSColor(hex: config.backgroundColorHex).withAlphaComponent(config.contentOpacity).setFill(); NSBezierPath(rect: bounds).fill()
        let path = NSBezierPath(rect: bounds.insetBy(dx: config.borderWidth / 2, dy: config.borderWidth / 2)); path.lineWidth = config.borderWidth
        NSColor(hex: config.borderColorHex).setStroke(); path.stroke()
        let font = NSFont.monospacedSystemFont(ofSize: min(config.fontSize, 14), weight: .regular)
        let sample = MetricSnapshot(
            sampledAt: .now, cpuPercent: 12.4, memoryUsedGiB: 8, memoryPercent: 42,
            gpuUsedGiB: 5.1, gpuPercent: 2.4, downloadBytesPerSecond: 102_400, uploadBytesPerSecond: 0
        )
        let text = MetricFormatter.hudText(snapshot: sample, config: config)
        let attributes: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor(hex: config.textColorHex)]
        let size = text.size(withAttributes: attributes)
        text.draw(at: NSPoint(x: max(0, (bounds.width - size.width) / 2), y: max(0, (bounds.height - size.height) / 2)), withAttributes: attributes)
    }
}
