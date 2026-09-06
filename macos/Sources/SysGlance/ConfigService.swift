import Foundation

final class ConfigService {
    private let fileManager = FileManager.default
    private let configURL: URL

    init(configURL: URL? = nil) {
        if let configURL {
            self.configURL = configURL
        } else {
            let support = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
                .appendingPathComponent("SysGlance", isDirectory: true)
            self.configURL = support.appendingPathComponent("config.ini")
        }
    }

    func load() -> (config: AppConfig, lastGood: AppConfig?) {
        guard let data = try? Data(contentsOf: configURL), let text = String(data: data, encoding: .utf8) else {
            return (AppConfig.recommended, nil)
        }
        let sections = parse(text)
        var config = makeConfig(from: sections["SysGlance"] ?? [:])
        config.normalize()
        let lastGood = sections["HUDLastGood"].map { values -> AppConfig in
            var value = makeConfig(from: values); value.normalize(); return value
        }
        return (config, lastGood)
    }

    func save(config: AppConfig, lastGood: AppConfig) throws {
        let directory = configURL.deletingLastPathComponent()
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        let text = "[SysGlance]\n\(serialize(config))\n[HUDLastGood]\n\(serialize(lastGood))"
        let tempURL = directory.appendingPathComponent("config.ini.tmp")
        try text.data(using: .utf8)!.write(to: tempURL, options: .atomic)
        if fileManager.fileExists(atPath: configURL.path) {
            _ = try fileManager.replaceItemAt(configURL, withItemAt: tempURL, backupItemName: nil, options: [])
        } else {
            try fileManager.moveItem(at: tempURL, to: configURL)
        }
    }

    private func parse(_ text: String) -> [String: [String: String]] {
        var result: [String: [String: String]] = [:]
        var section = "SysGlance"
        for raw in text.components(separatedBy: .newlines) {
            let line = raw.trimmingCharacters(in: .whitespacesAndNewlines)
            if line.hasPrefix("[") && line.hasSuffix("]") { section = String(line.dropFirst().dropLast()); continue }
            guard !line.hasPrefix("#"), let separator = line.firstIndex(of: "=") else { continue }
            result[section, default: [:]][String(line[..<separator])] = String(line[line.index(after: separator)...])
        }
        return result
    }

    private func makeConfig(from value: [String: String]) -> AppConfig {
        func bool(_ key: String, _ defaultValue: Bool) -> Bool { value[key].flatMap(Bool.init) ?? defaultValue }
        func int(_ key: String, _ defaultValue: Int) -> Int { value[key].flatMap(Int.init) ?? defaultValue }
        func cg(_ key: String, _ defaultValue: CGFloat) -> CGFloat { value[key].flatMap(Double.init).map { CGFloat($0) } ?? defaultValue }
        var c = AppConfig.recommended
        c.displayMode = DisplayMode(rawValue: value["displayMode"] ?? "") ?? c.displayMode
        c.refreshMilliseconds = int("refreshMilliseconds", c.refreshMilliseconds)
        c.showCPU = bool("showCPU", c.showCPU); c.showMemory = bool("showMemory", c.showMemory); c.showGPU = bool("showGPU", c.showGPU); c.showNetwork = bool("showNetwork", c.showNetwork)
        c.memoryDisplayMode = MemoryDisplayMode(rawValue: value["memoryDisplayMode"] ?? "") ?? c.memoryDisplayMode
        c.percentPrecision = PercentPrecision(rawValue: value["percentPrecision"] ?? "") ?? c.percentPrecision
        c.showNetworkArrows = bool("showNetworkArrows", c.showNetworkArrows)
        c.hudWidth = cg("hudWidth", c.hudWidth); c.hudHeight = cg("hudHeight", c.hudHeight); c.fontSize = cg("fontSize", c.fontSize); c.borderWidth = cg("borderWidth", c.borderWidth)
        c.borderColorHex = value["borderColorHex"] ?? c.borderColorHex; c.textColorHex = value["textColorHex"] ?? c.textColorHex; c.backgroundColorHex = value["backgroundColorHex"] ?? c.backgroundColorHex
        c.contentOpacity = cg("contentOpacity", c.contentOpacity); c.locked = bool("locked", c.locked); c.mouseThrough = bool("mouseThrough", c.mouseThrough)
        c.hudX = value["hudX"].flatMap(Double.init).map { CGFloat($0) }; c.hudY = value["hudY"].flatMap(Double.init).map { CGFloat($0) }
        return c
    }

    private func serialize(_ c: AppConfig) -> String {
        let lines: [String?] = [
            "displayMode=\(c.displayMode.rawValue)", "refreshMilliseconds=\(c.refreshMilliseconds)", "showCPU=\(c.showCPU)", "showMemory=\(c.showMemory)", "showGPU=\(c.showGPU)", "showNetwork=\(c.showNetwork)",
            "memoryDisplayMode=\(c.memoryDisplayMode.rawValue)", "percentPrecision=\(c.percentPrecision.rawValue)", "showNetworkArrows=\(c.showNetworkArrows)",
            "hudWidth=\(c.hudWidth)", "hudHeight=\(c.hudHeight)", "fontSize=\(c.fontSize)", "borderWidth=\(c.borderWidth)", "borderColorHex=\(c.borderColorHex)", "textColorHex=\(c.textColorHex)", "backgroundColorHex=\(c.backgroundColorHex)", "contentOpacity=\(c.contentOpacity)", "locked=\(c.locked)", "mouseThrough=\(c.mouseThrough)",
            c.hudX.map { "hudX=\($0)" }, c.hudY.map { "hudY=\($0)" }
        ]
        return lines.compactMap { $0 }.joined(separator: "\n") + "\n"
    }
}
