# AGENTS.md

本文件面向在本仓库工作的编码 Agent（以及人类协作者）。**先读本文件，再读 `docs/` 下的四份文档。**

## 项目是什么

**SnapOCR**：Windows 10/11 上的轻量截图 + 标注 + OCR 工具（C++20 / Qt 6.11 Widgets / MSVC / CMake + Ninja）。
当前阶段：**阶段二已完成并发布**（v0.1.0），**阶段三进行中**（最新正式版 v0.2.0：
主题一致性、稳定性审计、发布工程（含 Inno Setup 安装包）、性能验收、i18n 119 条、
拖拽导出、贴图取字/再标注）；多屏矩阵/IM 剪贴板/干净机器列为已知限制；
状态以 [完成情况](docs/04-完成情况.md) 为准。

- 文档索引与维护规则：`docs/README.md`
- 需求与验收：`docs/01-需求文档.md`
- 架构与契约：`docs/02-设计文档.md`
- 环境、构建、规范与任务：`docs/03-开发文档.md`
- 实际进度与验收证据：`docs/04-完成情况.md`

## 构建与测试（Windows）

```bat
:: Debug 构建 + ctest（单测 / offscreen 冒烟）
scripts\build.bat

:: Release 构建（跳过测试）
scripts\build.bat Release --no-tests

:: 手动验证
build\SnapOCR.exe --version
build\SnapOCR.exe --smoke-test
scripts\run.bat
```

WSL 下调用 Windows 工具链（本仓库日常开发环境）：

```bash
cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat Debug'
```

环境：`QTDIR` 默认 `E:\Qt6\6.11.1\msvc2022_64`（可覆盖）；VS 18/2022 的 C++ 工作负载；CMake ≥3.21；Ninja；Windows SDK 10.0.26100。
工具链实测：MSVC 19.51（v145）+ Qt 6.11.1 msvc2022_64 编译/链接/真实 `windows` 平台冒烟均通过。
CI 使用 Qt 6.10.3（aqtinstall 3.3.0 尚不支持 Qt 6.11 新仓库布局），源码保持 Qt 6.5+ 兼容。

## 架构地图（先建立这张图）

```text
main.cpp
  └─ app::Application        命令行解析 / 生命周期 / 后续托盘与热键装配
       ├─ capture::CaptureService       逐屏 GDI 捕获 → CaptureResult（物理像素）
       ├─ overlay::OverlayController    每屏一个覆盖窗口 + 全局选区模型
       ├─ editor::EditorController      标注编辑态 + 工具条
       ├─ annotate::AnnotationDocument  图元模型 / 命中测试 / render / QUndoStack
       ├─ ocr::OcrService → IOcrEngine  异步调度；WindowsOcrEngine / RapidOcrEngine(P1)
       ├─ ui::MainWindow / Settings / OcrResultDialog / Theme
       └─ platform::win32               热键 / DPI / 剪贴板 / 自启 / 捕获 / 控制台
```

依赖规则（硬约束）：

1. 业务模型（capture/annotate/ocr）不依赖窗口类，保证可单测。
2. UI 不直接调用 Win32 捕获/热键 API，必须经 `platform/win32` 或服务层。
3. OCR 业务代码不得直接 `#include <winrt/...>`，只能通过 `IOcrEngine`。
4. 跨模块只传值类型或信号；坐标以**物理像素**为权威（见设计文档 §4）。
5. 主线程禁止 >16ms 阻塞操作；捕获与 OCR 在 worker 线程完成、结果回主线程。

## 硬性约束

- **默认零联网、无遥测**；不引入后台网络请求（P2 功能需显式设计且默认关闭）。
- 不申请管理员权限；不装全局键盘钩子；不注入其他进程。
- 日志禁止记录 OCR 原文/剪贴板内容/用户截图路径中的敏感信息。
- 截图/导出一律按屏幕物理像素 1:1，禁止经过缩放或控件截图导出。
- 第三方依赖必须登记到 `docs/02-设计文档.md` §14（版本/许可证/用途）。
- 改需求先改 `docs/01-需求文档.md`；改契约先改 `docs/02-设计文档.md`（重大决策记 §17）；同步更新完成情况；禁止“先写代码再补文档”。

## 工作习惯

- 回答、注释、提交说明用**简体中文**；代码标识符/文件名用英文；UTF-8 无 BOM，中文直写（禁止 `\uXXXX`）。
- 代码风格以根 `.clang-format` 为准；提交遵循 Conventional Commits（见 `docs/03-开发文档.md` §11）。
- **提交需用户明确要求**：不要擅自 `git add`/`commit`/`push` 工作区修改；不要覆盖/回滚他人未提交的改动。
- 修复 bug 先补回归测试；新增核心逻辑必须有单测；发版前跑手测矩阵。
- 构建产物、日志、临时文件不得入库（见 `.gitignore`）。
- 不确定的工具链/平台行为，先在真实 Windows 环境验证（可用 `cmd.exe` / `powershell.exe` 从 WSL 调用），并在文档记录结论。

## 并行协作（Agent Teams）

阶段一起可按写域并行（见 `docs/03-开发文档.md` §9）：

| 写域 | 责任 |
|------|------|
| `src/capture/`、`src/overlay/`、`src/platform/win32/` | 捕获、覆盖层、DPI |
| `src/annotate/`、`src/editor/` | 标注模型与编辑器 |
| `src/ocr/` | OCR 引擎与基准 |
| `src/app/`、`src/ui/`、`tests/`、`scripts/`、`.github/` | 系统集成、测试、CI |

共享契约（`CaptureResult`、`SelectionModel`、`AnnotationDocument`、`IOcrEngine`、`Settings`）由 Lead 定稿后方可并行实现；跨线接口变更先改设计文档并通知所有写者；写域不重叠，集成前跑完整构建与测试。

## 常用检查

```bat
:: 全量构建 + 测试
scripts\build.bat

:: 仅构建 Release
scripts\build.bat Release --no-tests

:: 运行全部测试
ctest --test-dir build --output-on-failure
```

完成任何任务前，确认：构建通过、相关测试通过、文档同步、无敏感信息写入日志。

## 当前验收基线（阶段三 / v0.2.0）

```text
构建：MSVC 19.51 + Qt 6.11.1，/W4 零告警，全量编译通过
测试：ctest 37/37 passed（含坐标/选区/裁剪/标注/画布/热键/设置/自启/OCR/
      主题/历史/贴图/长截图/窗口识别/覆盖层主链路/编辑窗/i18n/资源度量/冒烟/UI 预览）
稳定性：SnapOCR.exe --stability-selftest → 20 轮 PASS（GDI 零增长）
性能：SnapOCR.exe --perf-selftest → PASS（热键→遮罩 67ms / 4K 捕获 57ms /
      区域裁剪 2ms / OCR 91ms / 空闲工作集 28MB）
冒烟：SnapOCR.exe --smoke-test → platform=windows, PASS, exit 0
聚合：SnapOCR.exe --selftest-all → PASS（4/4：smoke/capture/stability/perf）
真机：scripts\e2e-smoke.ps1 → ALL PASS（F1 截图 801x601 / F4 OCR 已知文本 / Esc）
注：构建后 build/ 已自包含 Qt（windeployqt POST_BUILD），无需 PATH 即可运行
CLI：SnapOCR.exe --version → SnapOCR 0.2.0
发布：v0.0.1 / v0.1.0 / v0.1.1 / v0.1.2 / v0.2.0
     （CI 打包绿色包 + Inno Setup 安装包，sha256 校验 + 独立运行验证）
```
