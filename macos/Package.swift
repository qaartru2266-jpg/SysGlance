// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "SysGlance",
    platforms: [.macOS(.v13)],
    products: [.executable(name: "SysGlance", targets: ["SysGlance"])],
    targets: [
        .executableTarget(name: "SysGlance"),
        .testTarget(name: "SysGlanceTests", dependencies: ["SysGlance"])
    ]
)
