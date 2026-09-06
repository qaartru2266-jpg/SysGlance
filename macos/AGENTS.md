# SysGlance 工作区约定

## 项目目标

SysGlance 是面向 macOS 13+、Apple Silicon 优先的原生 AppKit 状态 HUD。保持轻量、单进程、低干扰；不引入 WebView、Electron、后台守护进程或私有 GPU API。

## 代码与行为约束

- UI 只在主线程读取 `MetricSnapshot` 并绘制；系统指标查询只在 `MetricService` 的专用队列执行。
- 单项采样失败必须显示 `N/A`，不得影响其他指标或让应用退出。
- HUD 文字使用等宽字体。CPU、内存和网络字段的宽度是产品行为，修改格式化前必须同步更新 `docs/CURRENT-BEHAVIOR.md` 与测试。
- GPU 尚未有稳定的公开 API 方案；在完成 Apple Silicon PoC 前保持 `N/A` 降级，禁止接入未经验证的私有 API。
- 设置采用草稿语义：仅“应用”写盘、更新 HUD 和 LastGood；取消或关闭不得改动运行配置。
- 保存配置前调用 `AppConfig.normalize()`；`mouseThrough` 开启时必须同时锁定 HUD。

## 构建与交付

- 开发构建：`swift build`
- 打包可双击运行的本地版本：`zsh scripts/package-app.sh`
- 交付物：`dist/SysGlance.app`。应用为本地未签名包，不提交 `dist/`、`.build/` 或缓存目录。
- 完整 Xcode 可运行 XCTest；仅 Command Line Tools 环境至少应执行 `swift build`。

## 验证清单

- 打开应用后确认菜单栏只有图标，点击能看到完整摘要。
- 核对 HUD：仅右键拖动、左键和双击无操作、锁定、鼠标穿透、HUD/菜单栏模式切换。
- 休眠唤醒、网卡变化和显示器变化后，首周期允许 `N/A`，不得显示网络尖峰或丢失 HUD。
- 任何产品行为调整完成后，重新执行打包脚本并启动 `dist/SysGlance.app` 冒烟验证。
