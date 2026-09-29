# SysGlance v2.0.1

统一 Windows 与 macOS 的维护更新。

## 下载

- `SysGlance-v2.0.1-win-x64.zip`：Windows 10 22H2+ 与 Windows 11 的 x64 便携版。
- `SysGlance-v2.0.1-macos-arm64.zip`：macOS 13+ Apple Silicon 的 ARM64 版本，包含 `SysGlance.app`。

## 本次更新

- Windows：新增轻量诊断日志、上次退出状态和可选的异常退出自动恢复；不增加常驻守护进程。
- Windows 与 macOS：HUD 背景透明度与文字/边框透明度分离，默认背景完全透明。

## macOS 已知限制

- GPU 指标当前使用安全降级，始终显示 `N/A`；这不会影响 CPU、内存、网络、HUD 或菜单栏摘要。
- macOS 应用未签名、未 notarize。若 Gatekeeper 阻止首次打开，请在 Finder 中按住 Control 点击 `SysGlance.app`，再选择“打开”。

## 验证范围

- Windows：CMake Release 构建与 CTest。
- macOS：Apple Silicon ARM64 上的 XCTest、Release 打包、Info.plist 与 ZIP 内容校验。
