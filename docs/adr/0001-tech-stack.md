# ADR-0001：技术栈、屏幕捕获与 OCR 选型

- **状态**：已接受（Accepted）
- **日期**：2026-10-23
- **决策者**：项目负责人（poluce）与实现 Agent
- **相关文档**：[PLAN](../../PLAN.md)、[技术设计](../02-technical-design.md)

## 背景

要开发一个 Windows 10/11 上的截图 + 标注 + OCR 工具，核心要求：

1. 热键唤起快、截图 1:1 清晰、多屏/高 DPI 正确；
2. 标注编辑流畅，离线 OCR 可用；
3. 轻量、免安装、免管理员权限、绿色便携；
4. 长期可维护，能与本机既有工具链和用户技术背景匹配。

本机环境事实（2026-10-23 实测）：

- Windows 11（`10.0.26100`）+ WSL2；驱动器中已有 Qt 6.11.1 `msvc2022_64`、VS 18 Community（MSVC 19.51）、CMake 3.30.5、Ninja 1.12.1、Windows SDK 10.0.26100；
- **没有安装 .NET SDK**（仅运行时），C# 路线需额外安装；
- 系统 OCR 语言包 `zh-cn` 存在，WinRT `Windows.Media.Ocr` 可创建 `zh-Hans-CN` 引擎，实测一张中文界面截图识别耗时 59ms，但存在错字（准确率一般）；
- 用户既有项目以 Qt/C++（QWidget/QML）为主，仓库规范偏中文文档与 CMake。

## 决策

1. **语言与框架**：C++20 + Qt 6.11 Widgets（Core/Gui/Widgets/Network/Concurrent/Test）。
2. **构建工具链**：MSVC（VS 18 / v145）+ CMake ≥3.21 + Ninja（CMake 3.30 不支持 VS18 生成器，统一 Ninja）。
3. **屏幕捕获**：M1 用 Win32 GDI 逐显示器 `BitBlt` 到 DIB，按物理像素拼接；不使用 `QScreen::grabWindow`。
4. **覆盖层**：每块显示器一个置顶无边框覆盖窗口，共享全局选区模型，保证混合 DPI 下不缩放失真。
5. **OCR**：先接系统 `Windows.Media.Ocr`（C++/WinRT），通过 `IOcrEngine` 抽象预留 RapidOCR（ONNX Runtime）为 P1 可切换引擎。
6. **标注编辑**：QWidget + QPainter 自绘画布；文档模型 + 快照式 `QUndoStack` 撤销。
7. **打包**：`windeployqt` 绿色 zip；安装包/签名放 P2。
8. **交付**：本地仓库 + GitHub 私有远程；CI 使用 `windows-latest` + Qt 6.10.3（aqtinstall 兼容版本），本机开发使用 Qt 6.11.1，源码保持 Qt 6.5+ 兼容。

## 备选方案与取舍

### 语言/UI 框架

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **C++20 + Qt 6 Widgets** | 本机环境完备；原生性能；Win32/WinRT 互操作直接；绿色发布体积小；与用户既有技术栈一致 | UI 现代感需自行打磨；Qt 许可证义务 | ✅ 采用 |
| C# .NET 10 + WPF | 开发效率高、WinRT OCR 集成简单 | 本机无 .NET SDK；WPF 启动/内存略高；自绘截图覆盖层同样要 Win32 | ❌ 不采用 |
| Tauri 2 + React | UI 现代、迭代快 | 截图/热键/多屏 DPI 需大量原生桥接；包体与运行时更重 | ❌ 不采用 |
| C++/WinRT + WinUI 3 | 原生现代 UI | XAML 开发效率低于 Qt；无边框覆盖层/贴图窗定制受限 | ❌ 不采用 |

### 屏幕捕获

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **Win32 GDI BitBlt** | 简单、兼容广、可按物理像素逐屏抓取；无需额外依赖 | 独占全屏/DRM 黑屏；高分辨率拷贝成本 | ✅ M1 采用 |
| `QScreen::grabWindow` | 代码最少 | 受 Qt DPR 缩放影响，混合 DPI 易模糊/错位 | ❌ 不采用 |
| DXGI Desktop Duplication | 性能最好、可抓帧 | 实现复杂、独占全屏/GPU 场景限制、调试成本高 | 🕒 备选（性能不达标时） |
| Windows.Graphics.Capture | 现代 API | 需授权/绿框提示、异步帧池复杂，不适合“瞬间截图” | ❌ 不采用 |

### OCR

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **Windows.Media.Ocr** | 零依赖、离线、启动快、系统自带语言包；实测 59ms | 中文准确率一般；依赖用户安装 OCR 语言包 | ✅ 首选 |
| RapidOCR / PaddleOCR（ONNX） | 中文准确率更高、完全离线 | 需携带约 15MB 模型与 ONNX Runtime；集成工作量大 | 🕒 P1 可切换引擎 |
| Tesseract | 老牌、模型多 | 中文效果明显弱于前两者 | ❌ 不采用 |
| 在线 OCR API | 准确率高 | 违背离线/隐私定位 | ❌ 不采用 |

## 后果

**正面**

- 工具链开箱即用，M0 已实测编译、链接、真实 GUI 冒烟通过。
- 原生 C++/Qt 便于实现像素级覆盖层、逐屏 DPI 与后续贴图/长截图。
- OCR 引擎可替换，准确率问题有明确后路。

**负面 / 约束**

- Qt LGPLv3 合规：动态链接、附许可证、允许用户替换 Qt 库；若闭源分发需遵守相应义务。
- C++/WinRT 与 Qt 事件循环需要封装适配（`IOcrEngine` + worker 线程），增加一层复杂度。
- GDI 捕获存在独占全屏限制，需在产品文档中明确。
- 无 .NET SDK 既避免了额外安装，也意味着 UI 迭代依赖 C++/Qt，开发速度需靠架构与测试保证。

**后续动作**

- M1 验证 GDI 捕获 + 逐屏覆盖层在混合 DPI 下的正确性；性能不达标再评估 DXGI（另立 ADR）。
- M3 建立 OCR 基准集；字符召回 <85% 时启动 RapidOCR（M3.5/M5）。
- M5 确定绿色包结构、Qt 许可证声明与模型分发方式。

## 参考

- [Windows.Media.Ocr（Microsoft Learn）](https://learn.microsoft.com/en-us/uwp/api/windows.media.ocr.ocrengine)
- [RapidOCR（GitHub）](https://github.com/RapidAI/RapidOCR)
- [Qt 6 高 DPI 文档](https://doc.qt.io/qt-6/highdpi.html)
- [Windows DPI 感知](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
