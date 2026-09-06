# Windows 对齐记录

本文件记录 macOS 对已发布 Windows 版本的对齐状态。只以 GitHub Release tag 为准，不提前实现 Windows 开发中的未发布功能。

## 统一 Release：SysGlance v2.0.0

`v2.0.0` 是首个同时交付 Windows x64 与 macOS ARM64 的统一项目版本。它不代表 macOS 已新增 Windows `v1.0.3` 之后的行为；本目录的当前用户可见行为仍以 Windows `v1.0.3` 为对齐基线。macOS GPU 继续安全降级为 `N/A`，且 Release 应用未签名、未 notarize。

## 基线：Windows v1.0.1

比较来源：[Windows v1.0.1](https://github.com/qaartru2266-jpg/SysGlance/releases/tag/v1.0.1)，主线提交 `0640efc`。

| Windows v1.0.1 行为 | macOS 状态 | 说明 |
| --- | --- | --- |
| 托盘优先 | 已对齐 | macOS 使用仅图标菜单栏作为原生对应物。 |
| HUD 仅右键拖动 | 已对齐 | 左键和双击均不操作；锁定或鼠标穿透时不接收拖动。 |
| GPU 默认隐藏 | 已对齐 | 已有 macOS 配置不会被覆盖。 |
| 内存默认已用 GiB | 已对齐 | 已有 macOS 配置可继续使用百分比。 |
| 整数百分比、隐藏网络箭头 | 已对齐 | 仅影响新安装与“恢复推荐 HUD”。 |
| CPU、内存、网络固定宽度 | 已对齐 | 网络字段为四位数值加 `K/M`，首样本 `N/A` 同宽。 |
| 高 DPI 设置控件 | 原生适配 | AppKit 使用可滚动设置窗口与系统原生下拉控件；需在实际 Retina/外接屏持续手工验证。 |
| 网卡/GPU 设备选择 | 后续 | 不属于本轮基础对齐。 |
| 自动启动 | 后续 | 不属于本轮基础对齐。 |

## 对齐：Windows v1.0.3

比较来源：[Windows v1.0.3](https://github.com/qaartru2266-jpg/SysGlance/releases/tag/v1.0.3)，相对基线 `v1.0.1` 的提交为 `d59f8ea`、`b7b51ce`。

| Windows v1.0.3 变化 | macOS 状态 | 说明 |
| --- | --- | --- |
| 设置窗口显示运行版本 | 已对齐 | 标题与内容显示 `macOS v1.0.3`；本地应用包 `Info.plist` 同步为 `1.0.3`。 |
| 配置保存/启动项操作反馈 | 部分对齐 | macOS 无启动项设置；“应用”后会区分写盘成功和“仅本次已应用、重启可能恢复旧设置”。 |
| 网络初始化与不可用状态区分 | 已对齐 | HUD 继续同宽显示 `N/A`；菜单摘要分别显示 `Network initializing` 和 `Network N/A`。 |
| 休眠、设备/拓扑变化后重建采样基线 | 已对齐 | 唤醒、显示器/缩放变化及 `en*` 接口集合变化会重建基线，避免陈旧速率和尖峰。 |
| GPU 设备清单与重新初始化 | 平台差异 | macOS 当前不接入未经验证的 GPU 采样 API，因此继续安全降级为 `N/A`。 |

## 后续对齐流程

1. Windows 发布新的稳定 `v1.0.x` tag 后，比较该 tag 与上一个已对齐 tag 的产品文档、源代码、测试和 Release 说明。
2. 在此文件新增差异表，标注“已对齐 / 平台不适用 / 后续”，再修改 macOS。
3. 在本地 macOS Git 中提交代码、测试、文档和构建结果。作为 v1.0.3 的交接准备，源码镜像会推送到 GitHub 独立分支 `codex/macos-v1.0.3`，仅新增 `macos/`，绝不直接修改 `main` 或 Windows 文件。
4. Windows `v1.0.5` 完成后，由主开发电脑审阅并决定将 `macos/` 合入主线；交付依据仍是 macOS 源码、此文件、测试结果和提交记录。
