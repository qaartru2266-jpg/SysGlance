import AppKit

final class HudPanel: NSPanel {
    private let hudView = HudView(frame: .zero)
    private var rightDragStartCursor: NSPoint?
    private var rightDragStartOrigin: NSPoint?
    var onPositionChanged: ((NSPoint) -> Void)?

    init(config: AppConfig) {
        super.init(contentRect: NSRect(origin: .zero, size: NSSize(width: config.hudWidth, height: config.hudHeight)),
                   styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        isFloatingPanel = true
        level = .floating
        collectionBehavior = [.canJoinAllSpaces, .stationary]
        isOpaque = false
        hasShadow = false
        // HUD movement intentionally follows Windows v1.0.1: secondary-button drag only.
        isMovableByWindowBackground = false
        backgroundColor = .clear
        hidesOnDeactivate = false
        contentView = hudView
        hudView.onSecondaryMouseDown = { [weak self] event in self?.beginRightDrag(event) }
        hudView.onSecondaryMouseDragged = { [weak self] event in self?.continueRightDrag(event) }
        hudView.onSecondaryMouseUp = { [weak self] in self?.finishRightDrag() }
        apply(config: config, reposition: true)
    }

    func apply(config: AppConfig, reposition: Bool = false) {
        let oldOrigin = frame.origin
        setContentSize(NSSize(width: config.hudWidth, height: config.hudHeight))
        hudView.config = config
        ignoresMouseEvents = config.mouseThrough
        if reposition { setFrameOrigin(position(for: config)) }
        else { setFrameOrigin(clamped(origin: oldOrigin)) }
        orderFrontRegardless()
    }

    func update(snapshot: MetricSnapshot, config: AppConfig) {
        hudView.text = MetricFormatter.hudText(snapshot: snapshot, config: config)
    }

    func currentOrigin() -> NSPoint { frame.origin }

    private func position(for config: AppConfig) -> NSPoint {
        guard let screen = NSScreen.main else { return .zero }
        if let x = config.hudX, let y = config.hudY { return clamped(origin: NSPoint(x: x, y: y), on: screen) }
        let visible = screen.visibleFrame
        return NSPoint(x: visible.maxX - config.hudWidth - 24, y: visible.maxY - config.hudHeight - 24)
    }

    private func clamped(origin: NSPoint, on screen: NSScreen? = nil) -> NSPoint {
        let targetScreen = screen ?? NSScreen.screens.first(where: { $0.frame.intersects(frame) }) ?? NSScreen.main
        guard let targetScreen else { return origin }
        let visible = targetScreen.visibleFrame
        let maxX = max(visible.minX, visible.maxX - frame.width)
        let maxY = max(visible.minY, visible.maxY - frame.height)
        return NSPoint(x: min(max(origin.x, visible.minX), maxX), y: min(max(origin.y, visible.minY), maxY))
    }

    private func beginRightDrag(_ event: NSEvent) {
        guard !ignoresMouseEvents, !hudView.config.locked else { return }
        rightDragStartCursor = convertPoint(toScreen: event.locationInWindow)
        rightDragStartOrigin = frame.origin
    }

    private func continueRightDrag(_ event: NSEvent) {
        guard !ignoresMouseEvents, !hudView.config.locked,
              let startCursor = rightDragStartCursor, let startOrigin = rightDragStartOrigin else { return }
        let cursor = convertPoint(toScreen: event.locationInWindow)
        setFrameOrigin(NSPoint(x: startOrigin.x + cursor.x - startCursor.x, y: startOrigin.y + cursor.y - startCursor.y))
    }

    private func finishRightDrag() {
        guard rightDragStartOrigin != nil else { return }
        rightDragStartCursor = nil
        rightDragStartOrigin = nil
        setFrameOrigin(clamped(origin: frame.origin))
        onPositionChanged?(frame.origin)
    }
}

private final class HudView: NSView {
    var config = AppConfig.recommended { didSet { needsDisplay = true } }
    var text = "" { didSet { needsDisplay = true } }
    var onSecondaryMouseDown: ((NSEvent) -> Void)?
    var onSecondaryMouseDragged: ((NSEvent) -> Void)?
    var onSecondaryMouseUp: (() -> Void)?

    override var mouseDownCanMoveWindow: Bool { false }

    override func mouseDown(with event: NSEvent) { }

    override func rightMouseDown(with event: NSEvent) { onSecondaryMouseDown?(event) }

    override func rightMouseDragged(with event: NSEvent) { onSecondaryMouseDragged?(event) }

    override func rightMouseUp(with event: NSEvent) { onSecondaryMouseUp?() }

    override func draw(_ dirtyRect: NSRect) {
        let rect = bounds.insetBy(dx: config.borderWidth / 2, dy: config.borderWidth / 2)
        NSColor(hex: config.backgroundColorHex).withAlphaComponent(config.contentOpacity).setFill()
        NSBezierPath(rect: bounds).fill()
        if config.borderWidth > 0 {
            let path = NSBezierPath(rect: rect)
            path.lineWidth = config.borderWidth
            NSColor(hex: config.borderColorHex).withAlphaComponent(config.contentOpacity).setStroke()
            path.stroke()
        }
        let font = NSFont(name: "SF Mono", size: config.fontSize) ?? NSFont.monospacedSystemFont(ofSize: config.fontSize, weight: .regular)
        let attributes: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor(hex: config.textColorHex).withAlphaComponent(config.contentOpacity)]
        let size = text.size(withAttributes: attributes)
        text.draw(
            at: NSPoint(x: max(0, (bounds.width - size.width) / 2), y: max(0, (bounds.height - size.height) / 2)),
            withAttributes: attributes
        )
    }
}
