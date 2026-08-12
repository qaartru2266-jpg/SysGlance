# SysGlance 开发记录

## 当前状态（2026-08-12）

本轮已完成“稳定性与体验收敛”实现，实际发布目标为 `build\Release\SysGlance.exe`。最近一次构建通过，`SysGlanceMathTests` 已通过 CTest。

当前主体验是 HUD 与托盘。任务栏信息条保留为实验功能，不应继续为“看起来像系统任务栏的一部分”投入实现；它绝不能占据或修改系统任务栏。

## 已实现的关键决策

### 配置和设置

- `AppConfig` 是唯一运行配置；`ConfigService::Normalize` 在加载和应用后统一约束无效值、刷新间隔和互斥状态。
- 设置窗口采用 `settingsDraft_`：控件修改和取色只改草稿；实际 `config_` 只在“应用”时更新。
- 配置持久化位置为 `%LOCALAPPDATA%\SysGlance\config.ini`。
- `HUDLastGood` INI 节保存上次可渲染布局，用于恢复。不要将不具备最小文本空间的自由尺寸写为“上次可用布局”。
- 新增配置必须同步覆盖：`AppConfig`、`Load`、`Save`、`Normalize`、设置控件回填、草稿读取、应用到运行服务及产品说明。

### HUD 和渲染

- HUD 由 `hudFrameWindow_`（不透明边框）和 `hudWindow_`（带透明度内容）组成。边框与内容透明度必须分离。
- `CalculateHudLayout` 是 HUD 外框、内容区域、像素边框和可渲染性判定的唯一布局入口。拖动、应用尺寸、DPI 变化、显示器变化、恢复布局都应走它。
- 指标刷新 (`WM_APP + 1`) 只更新快照、托盘提示和重绘，不能调用 `PositionHudSurface`，否则会导致不必要的窗口移动和卡顿。
- HUD 使用 Consolas 和固定文本槽位。禁止为实时读数重新测量/自动扩张窗口；网络仅 `KB/MB`（≥100 KiB/s 切 MB），需保持固定宽度。
- 双击 HUD 修改 `hudNetworkOnly` 并保存，短暂反馈。锁定或鼠标穿透时需保持无交互。

### 采样与设备选择

- `MetricService` 是唯一采样入口，后台线程生成 `MetricSnapshot`，UI 通过 `Snapshot()` 读取。
- 网卡选择由 `selectedNetworkLuid`、`includeVirtualNetworkInterfaces` 控制。切换时递增选择代次、清除网络基线，首周期 `networkReady=false`。
- 选择单个接口时允许该接口即使是虚拟接口；默认聚合时只纳入已连接物理接口，勾选后纳入虚拟/VPN 接口，仍排除回环。
- GPU 选择由 `selectedGpuLuid` 控制。PDH 多实例名称中解析 LUID；GPU 容量由 DXGI 适配器列表按 LUID 提供。
- 所选设备消失时 UI 侧回退到 LUID=0（默认聚合）并持久化。GPU PDH 连续失败时 `GpuState` 重建查询。

## 已知限制与后续验证

以下事项不是当前阻塞，但下一位开发者在继续修改相关路径时必须复验：

1. GPU PDH 实例的 LUID 命名依赖 Windows 性能计数器格式；需在 Intel、NVIDIA、AMD、多 GPU、RDP 与驱动重启环境手工验证筛选结果。
2. 网络“包含 VPN/虚拟接口”需要在常见 VPN、Hyper-V、WSL 与虚拟机网卡环境验证，确认没有重复计数。
3. 设置窗口目前使用绝对坐标的 Win32 控件布局；高 DPI 下应检查 125%、150% 和更高缩放是否发生重叠。
4. 自由尺寸故意不设产品硬下限。必须验证极小 HUD 后，推荐布局和上次可用布局能恢复可见状态。
5. `scripts\build-release.ps1` 使用本开发机的 Visual Studio / CMake 位置，并会复制到 `build\Release`；需要在其他环境使用时先调整工具发现逻辑。脚本不负责关闭正在运行的可执行文件。

## 验证基线

每次修改后按影响范围执行：

| 改动 | 最低验证 |
| --- | --- |
| 计算、格式化、配置 | CTest。 |
| 采样、网络、GPU | CTest + 手工检查首样本 `N/A`、设备切换、断开/恢复。 |
| HUD/UI | CTest + 手工检查显示、拖动、锁定、鼠标穿透、设置取消、应用、重启恢复。 |
| 布局/DPI | 上述检查 + 至少 125% 和 150% DPI、双显示器和任务栏不同边。 |

开发机的已知可用构建命令：

```powershell
cmd /c 'call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && "C:\Program Files\CMake\bin\cmake.exe" --build D:\devray\SysGlance\build-nmake && "C:\Program Files\CMake\bin\ctest.exe" --test-dir D:\devray\SysGlance\build-nmake --output-on-failure'
```

或者使用：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
```

## 近期变更时间线

### 2026-08-12

- 网络速率取消原始字节单位：闲置为 `0.0KB`，≥100 KiB/s 显示为 MB（约 `0.1MB`）；`FormatNetworkRate` 可单测。
- 完成设置草稿、真实预览、推荐布局与上次可用布局恢复。
- 完成网卡/GPU 选择、网络长间隔基线重置、GPU 性能计数器失败重建和设备消失回退。
- 完成统一 `HudLayout` 路径，移除每秒采样时的 HUD 重定位。
- 为任务栏模式增加“实验功能”标识。
- 固定网络数值和单位槽位，修复单位切换时 HUD 文本整体伸缩。
- 新增单一 Release 构建、CTest、复制脚本。

### 2026-08-11

- 建立原生 Win32 / Direct2D / DirectWrite MVP。
- 完成托盘、HUD、基本设置、CPU/内存/GPU/网络采样和基础 CTest。
