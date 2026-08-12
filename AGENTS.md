# SysGlance 协作指南

本文件是接手本项目时的工作契约。开始改动前，先阅读 [产品说明](docs/PRODUCT.md) 和 [开发记录](docs/DEVELOPMENT.md)，特别是“已知限制与后续验证”。

## 项目边界

- 这是一个 C++20 / Win32 原生 Windows 工具，保持单进程、低依赖。
- 不引入 Qt、Electron、WebView、后台服务或 Explorer 注入。
- 任务栏模式只能是任务栏附近的信息条，不能占用或修改系统任务栏；它是实验功能。
- 目标平台是 Windows 10 22H2+ 与 Windows 11，x64 优先。
- 当前产品主体验是 HUD 与托盘；不要为伪任务栏融合牺牲稳定性。

## 目录职责

| 路径 | 职责 |
| --- | --- |
| `src/common.h` | 共享快照、配置、设备信息和纯计算辅助类型。 |
| `src/metrics.*` | 后台采样线程、Windows 指标提供逻辑、设备枚举与选择。 |
| `src/config.*` | `%LOCALAPPDATA%\SysGlance\config.ini` 的加载、规范化、持久化和自动启动。 |
| `src/ui.*` | 托盘、HUD、实验信息条、设置窗口、Direct2D/DirectWrite 绘制。 |
| `tests/` | 不依赖 UI 的计算与边界测试。 |
| `docs/` | 产品口径、开发状态和验证记录。 |
| `scripts/build-release.ps1` | 当前开发机的 NMake Release 构建、CTest 和可执行文件复制流程。 |

## 修改规则

### 配置

- 新增可持久化设置时，必须同时修改 `AppConfig`、`ConfigService::Load`、`ConfigService::Save`、`ConfigService::Normalize`、设置控件回填、草稿读取和应用逻辑。
- 设置页必须维持草稿语义：预览和取色不能直接修改 `config_`、写 INI 或更新 HUD；只有“应用”可提交。
- 不要覆盖用户已有配置或构建目录内容，除非任务明确要求。特别是不要为“修复显示”擅自重置用户 HUD 尺寸或可见指标。

### 指标采样

- 指标采样仅放在 `MetricService`；UI 线程只读取 `Snapshot()`，不得在绘制路径中查询性能接口。
- 所有指标提供方都必须允许失败，使用 `N/A` 降级，不能终止应用。
- 网络切换接口、网络长间隔、设备消失后必须重置基线；首个恢复周期不能输出速度尖峰。
- GPU 默认值始终是聚合；新增设备选择不能改变旧配置的默认聚合行为。
- GPU 内存统一称为“GPU 内存”，不要无条件称“独立显存”。

### HUD、文本与交互

- HUD 是外框窗口和内容窗口的组合。修改尺寸、边框或 DPI 时，必须检查 `CalculateHudLayout`、`PositionHudSurface`、`UpdateHudFrameRegion` 三者。
- 尺寸、拖动、DPI 变化、显示器变化、恢复布局只走统一布局路径；指标刷新只能重绘，不得每秒重新定位 HUD。
- HUD 文本需保持紧凑稳定。使用固定槽位、等宽字体和尾部补位；不得在 `C`、`G` 前加入补空格，也不要因为单位切换令总文本宽度变化。
- 百分比精度、内存口径、箭头显示必须由配置决定。托盘摘要可使用完整单位/百分号，HUD 保持紧凑规则。
- 双击网络精简、拖动、锁定和鼠标穿透属于现有行为。任何改动都要检查它们的互斥关系：锁定/穿透后不抢鼠标。

## 构建与验证

常规构建：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

本开发机也可使用已配置的 `build-nmake`：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
```

修改采样、配置或格式化逻辑后，至少运行 CTest。修改 UI 后，额外手工确认：HUD 可显示、拖动、设置取消不改变实际 HUD、应用后持久化、重启恢复、锁定/穿透、双击网络精简。修改设备和布局后，按开发记录补充网卡切换、GPU 选择、睡眠唤醒、DPI 和多显示器验证。

部署到 `build\Release\SysGlance.exe` 前，先确认正在运行的同路径进程已退出；Windows 会锁定可执行文件。不要把调试构建误当作已部署版本。

## 数据口径注意事项

- 网络 HUD 单位为字节每秒，任务管理器常用 Mbps；对照时需要换算。
- GPU 内存计数器与厂商驱动软件的展示口径可能不同，且可能包含共享内存。
- 多 GPU 下默认显示聚合值；选择单 GPU 时，PDH 实例依靠 LUID 过滤，需在目标环境实测。
