# 开发、测试与发布

## 当前对齐基线

本机实现当前以 Windows GitHub Release `v1.0.3` 为产品行为基线：菜单栏优先、隐藏 GPU、内存 GiB、整数百分比、隐藏网络箭头，并且 HUD 仅响应右键拖动。已有 macOS 配置不会因这些默认值变更而被重置。v1.0.3 还要求睡眠唤醒与显示器变化后重新建立采样基线，并区分网络初始化与不可用状态。

## 统一发布版本

`v2.0.0` 是 SysGlance 的首个统一 Windows 与 macOS Release。它提升 macOS 设置窗口、应用包与 Release 资产的版本号，但不改变这里记录的 Windows `v1.0.3` 行为对齐基线。SwiftPM manifest 保持 Swift tools 5.10 兼容，以适配当前 GitHub `macos-14` ARM64 runner；GitHub Actions 会运行 `swift test`、打包 `SysGlance.app` 并将其作为未签名、未 notarize 的 macOS Release ZIP 发布。

## 前置条件

- macOS 13+，Apple Silicon 优先。
- Swift Package Manager 可用于构建运行应用。
- 完整 Xcode 只在 XCTest、代码签名、notarization 或 Xcode 图形化调试时需要。

## 常用命令

```sh
# Debug 构建
swift build

# 启动开发构建
swift run SysGlance

# 打包并运行 Release 应用
zsh scripts/package-app.sh
open dist/SysGlance.app
```

如当前环境限制 Swift 默认缓存，构建命令前可设置：

```sh
CLANG_MODULE_CACHE_PATH="$PWD/.build-cache/clang" \
SWIFTPM_MODULECACHE_OVERRIDE="$PWD/.build-cache/swiftpm" swift build
```

完整 Xcode 已安装时运行：

```sh
swift test
```

## 本地发布流程

1. 执行 `swift build`，确保 Debug 目标能链接。
2. 执行 `zsh scripts/package-app.sh`，该脚本会构建 Release 二进制、创建 `dist/SysGlance.app` 并校验 `Info.plist`。
3. 使用 `open -n dist/SysGlance.app` 启动，确认菜单栏图标、HUD、设置窗口和退出菜单可用。
4. 本地构建包未签名；若 Gatekeeper 阻止首次启动，Control 点击应用并选择“打开”。发布给其他用户前应另行完成 Developer ID 签名和 notarization。

## 配置

配置路径：`~/Library/Application Support/SysGlance/config.ini`。

- `SysGlance` section 保存当前配置。
- `HUDLastGood` section 保存最后一次成功应用的 HUD 布局。
- 不要手动编辑配置后仍让旧进程运行；重启应用或通过设置“应用”使变更生效。
