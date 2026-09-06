# SysGlance for macOS

原生 AppKit 系统状态 HUD：CPU、内存、GPU（安全降级）和网络速率，以及仅图标的菜单栏入口和展开后的实时摘要。HUD 中的网络单位使用紧凑的 `K` / `M` 标记。

当前 HUD 格式、交互和已知限制见 [当前产品行为](docs/CURRENT-BEHAVIOR.md)；与 Windows Release 的对齐状态见 [Windows 对齐记录](docs/WINDOWS-ALIGNMENT.md)；构建、测试和本地发布流程见 [开发文档](docs/DEVELOPMENT.md)。

## 运行

在仓库根目录运行：

```sh
swift run SysGlance
```

执行测试：

```sh
swift test
```

创建可双击打开的 Release 应用包：

```sh
zsh scripts/package-app.sh
open dist/SysGlance.app
```

应用包位于 `dist/SysGlance.app`。它是本地未签名版本，首次打开若被 Gatekeeper 拦截，请在 Finder 中按住 Control 点击应用并选择“打开”。

配置保存在 `~/Library/Application Support/SysGlance/config.ini`。网络上下行箭头默认关闭，可在设置中按需打开。首版为本地非沙盒构建，GPU 指标在完成目标硬件采样 PoC 前会显示 `N/A`，不会影响其他指标。完整 Xcode 仅用于运行 XCTest 和签名/归档，并非运行此应用的前置条件。
