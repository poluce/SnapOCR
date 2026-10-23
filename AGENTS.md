# AGENTS.md

本文件面向在本仓库工作的编码 Agent（以及人类协作者）。**先读本文件，再读 `PLAN.md` 与 `docs/`。**

## 项目是什么

**SnapOCR**：Windows 10/11 上的轻量截图 + 标注 + OCR 工具（C++20 / Qt 6.11 Widgets / MSVC / CMake + Ninja）。
当前阶段：**M0 已完成**（仓库、计划文档、可编译骨架、CI），下一阶段 M1 屏幕捕获与选区。

- 总计划与决策：`PLAN.md`
- 文档索引与权威顺序：`docs/README.md`
- 需求编号与验收：`docs/01-product-requirements.md`
- 架构与契约：`docs/02-technical-design.md`
- 任务分解：`docs/03-roadmap.md`
- 工程规范：`docs/04-dev-conventions.md`

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
4. 跨模块只传值类型或信号；坐标以**物理像素**为权威（见技术设计 §4）。
5. 主线程禁止 >16ms 阻塞操作；捕获与 OCR 在 worker 线程完成、结果回主线程。

## 硬性约束

- **默认零联网、无遥测**；不引入后台网络请求（P2 功能需显式设计且默认关闭）。
- 不申请管理员权限；不装全局键盘钩子；不注入其他进程。
- 日志禁止记录 OCR 原文/剪贴板内容/用户截图路径中的敏感信息。
- 截图/导出一律按屏幕物理像素 1:1，禁止经过缩放或控件截图导出。
- 第三方依赖必须登记到 `docs/02-technical-design.md` §14（版本/许可证/用途）。
- 改需求先改 `docs/01` 与 `PLAN.md`；改契约先改 `docs/02` 或补 ADR；禁止“先写代码再补文档”。

## 工作习惯

- 回答、注释、提交说明用**简体中文**；代码标识符/文件名用英文；UTF-8 无 BOM，中文直写（禁止 `\uXXXX`）。
- 代码风格以根 `.clang-format` 为准；提交遵循 Conventional Commits（见 `docs/04`）。
- **提交需用户明确要求**：不要擅自 `git add`/`commit`/`push` 工作区修改；不要覆盖/回滚他人未提交的改动。
- 修复 bug 先补回归测试；新增核心逻辑必须有单测；发版前跑手测矩阵。
- 构建产物、日志、临时文件不得入库（见 `.gitignore`）。
- 不确定的工具链/平台行为，先在真实 Windows 环境验证（可用 `cmd.exe` / `powershell.exe` 从 WSL 调用），并在文档记录结论。

## 并行协作（Agent Teams）

M1 起可按写域并行（见 `docs/03-roadmap.md` §8）：

| 写域 | 责任 |
|------|------|
| `src/capture/`、`src/overlay/`、`src/platform/win32/` | 捕获、覆盖层、DPI |
| `src/annotate/`、`src/editor/` | 标注模型与编辑器 |
| `src/ocr/` | OCR 引擎与基准 |
| `src/app/`、`src/ui/`、`tests/`、`scripts/`、`.github/` | 系统集成、测试、CI |

共享契约（`CaptureResult`、`SelectionModel`、`AnnotationDocument`、`IOcrEngine`、`Settings`）由 Lead 定稿后方可并行实现；跨线接口变更先改技术设计并通知所有写者；写域不重叠，集成前跑完整构建与测试。

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

## 当前验收基线（M0）

```text
构建：MSVC 19.51 + Qt 6.11.1，14 个构建步骤通过
测试：ctest 2/2 passed（snapocr_tests、snapocr_smoke）
冒烟：SnapOCR.exe --smoke-test → platform=windows, PASS, exit 0
CLI：SnapOCR.exe --version → SnapOCR 0.1.0
```
