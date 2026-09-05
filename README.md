# SysGlance

SysGlance 是一个供个人使用的原生 Windows 轻量系统监控工具。它以单进程、低依赖、低常驻资源为目标，实时展示 CPU、内存、GPU 和网络状态。

当前推荐使用桌面 HUD 或托盘摘要。任务栏信息条仍保留，但属于实验功能：它只会显示在任务栏附近，绝不修改、注入或占用 Windows 系统任务栏。

## 当前能力

- 托盘图标、悬停摘要和右键菜单。
- 可拖动的矩形 HUD：锁定、鼠标穿透、双击切换网络精简模式。
- CPU、物理内存、GPU 利用率、GPU 内存、网络下载/上传的统一采样。
- 单独控制每一项是否显示；内存和 GPU 内存可显示实际用量或百分比。
- 百分比可显示一位小数或整数；网络箭头可隐藏。
- HUD 的宽高、字体、边框厚度、透明度及背景/文字/边框颜色均可配置，并提供三套预设。
- 网卡选择（默认汇总所有已连接物理网卡，可选择单卡并包含 VPN/虚拟接口）与 GPU 选择（默认聚合所有适配器）。
- 设置采用草稿模式：修改仅影响预览；点击“应用”才更新 HUD 并写入配置。
- 可恢复推荐 HUD，或恢复上一次成功渲染的可用布局。

## 使用

双击根目录的 `启动 SysGlance.vbs` 可无窗口启动程序；也可以直接运行 `build\Release\SysGlance.exe`。启动后，右键托盘图标可切换模式或打开设置。

- HUD 未锁定且未开启鼠标穿透时可以拖动。
- 双击 HUD 会在完整显示与仅网络显示之间切换，并短暂显示切换反馈。
- 网络 HUD 的单位是字节每秒，界面省略 `/s`；与任务管理器的 Mbps 对照时，需要按 8 倍换算。

配置文件位于 `%LOCALAPPDATA%\SysGlance\config.ini`。

## 构建与验证

需要 Visual Studio 2022（MSVC）、Windows 10/11 SDK 和 CMake 3.24+。

常规 CMake 构建：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

当前开发机可使用单一脚本完成 NMake Release 构建、CTest 与部署：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
```

部署前若已有 SysGlance 正在运行，请先正常退出，避免 Windows 锁定可执行文件。

## 便携发布与 GitHub Release

发布包是单个 x64 可执行文件加说明文档，不携带个人配置。构建发布候选包：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-portable.ps1 -Version 1.0.3-preview.1
```

它会在 `dist\` 下生成 `SysGlance-<版本>-win-x64.zip`，且不替换本机正在使用的 `build\Release\SysGlance.exe`。发布版本静态链接 MSVC 运行库；支持的 Windows 10/11 系统只需使用自带的系统 DLL。

仓库已包含 [GitHub Actions 发布工作流](.github/workflows/release.yml)：推送形如 `v1.0.3` 的 Git 标签后，GitHub 会在 Windows 环境构建、运行 CTest、打包并创建对应 Release。首次使用建议先创建一个仅供测试的仓库，推送 `v1.0.3-preview.1` 标签验证流程。

GPU 指标依赖 Windows 性能计数器和驱动。Intel、NVIDIA、AMD 的常规本地桌面会尝试采样；虚拟机、RDP、旧驱动或驱动重启后可能显示 `N/A`，但应用会继续运行并定期恢复查询。CPU、内存、HUD 和网络不依赖 GPU 计数器。

## 文档

- [产品说明](docs/PRODUCT.md)：产品范围、显示语义和数据口径。
- [开发记录](docs/DEVELOPMENT.md)：当前实现状态、重要决策、已知限制与验证清单。
- [协作指南](AGENTS.md)：接手开发时必须遵守的边界和验证要求。
- [跨电脑测试说明](docs/CROSS_DEVICE_TEST.md)：在另一台 Windows 电脑下载、验证并反馈的步骤。
