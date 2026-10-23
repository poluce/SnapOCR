# SnapOCR

Windows 10/11 上的轻量**截图 + 标注 + OCR**工具（开发中）。

SnapOCR 的目标是“快捷、顺手、离线”：按下热键冻结屏幕、框选区域，随即画框/箭头/画笔/文字/马赛克，或一键提取图中文字，复制、保存、贴图都在悬浮工具条上完成。所有能力本地运行，默认不联网、不上传、不需要管理员权限。

[![Build](https://github.com/poluce/SnapOCR/actions/workflows/build.yml/badge.svg)](https://github.com/poluce/SnapOCR/actions/workflows/build.yml)

## 当前状态

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| M0 | 仓库、计划文档、可编译骨架、CI | ✅ 已完成 |
| M1 | 屏幕捕获、逐屏覆盖层、选区交互、复制/保存 | ⏳ 下一阶段 |
| M2 | 标注编辑器（9 类工具、撤销重做、导出） | 规划中 |
| M3 | Windows OCR、结果窗、F4 屏取字、准确率基准 | 规划中 |
| M4 | 托盘、设置、热键自定义、单实例、自启、贴图、历史 | 规划中 |
| M5 | RapidOCR 引擎、长截图、性能达标、发布打包 | 规划中 |

功能范围、验收标准与任务分解见 [PLAN.md](PLAN.md) 与 [docs/03-roadmap.md](docs/03-roadmap.md)。

## 功能规划（P0 = MVP）

| 模块 | P0 能力 |
|------|---------|
| 截图 | 区域截图（`F1`）、全屏/活动窗口、冻结画面、尺寸坐标提示、多显示器与 100%–200% DPI、物理像素 1:1 |
| 标注 | 矩形、椭圆、直线、箭头、画笔、文字、序号、高亮、马赛克；选中/移动/删除；撤销重做；颜色/线宽/字号 |
| 输出 | 复制到剪贴板、保存 PNG/JPG、另存为 |
| OCR | 截图后提取文字、`F4` 屏幕框选直接取字、结果窗选择/复制、离线识别、语言包缺失提示 |
| 系统 | 全局热键（可配置、冲突检测）、托盘常驻、单实例、开机自启、设置持久化、深浅主题、本地日志 |

P1（贴图、历史、RapidOCR、放大镜、长截图…）与 P2（录屏、二维码、翻译、安装包/签名…）规划见 [产品需求](docs/01-product-requirements.md)。

## 技术栈

- **语言/框架**：C++20，Qt 6.11 Widgets（Core / Gui / Widgets / Network / Concurrent / Test）
- **编译**：MSVC 19.51（VS 18）、CMake ≥ 3.21、Ninja（`/utf-8`，C++20）
- **OCR**：Windows 内置 OCR（`Windows.Media.Ocr`，C++/WinRT）优先；RapidOCR（ONNX Runtime）作为 P1 可切换引擎
- **平台能力**：Win32 `RegisterHotKey`、GDI 屏幕捕获、PerMonitorV2 DPI、`QLocalServer` 单实例
- **CI**：GitHub Actions `windows-latest` + `install-qt-action` + `ctest`（CI 固定 Qt 6.10.3，本机开发用 6.11.1）

## 环境要求

- Windows 10 1903+ / Windows 11，x64
- Qt 6.11.1 `msvc2022_64`（默认安装路径 `E:\Qt6\6.11.1\msvc2022_64`，可用 `QTDIR` 覆盖）
- Visual Studio 2022 或 2026，安装“使用 C++ 的桌面开发”工作负载
- CMake ≥ 3.21、Ninja（本机默认取 Qt 自带 `Tools/CMake_64`、`Tools/Ninja`）
- Windows SDK 10.0.26100（含 C++/WinRT 头文件）

## 构建与运行

```bat
:: 构建 Debug + 运行单元测试/冒烟测试
scripts\build.bat

:: Release 构建（跳过测试）
scripts\build.bat Release --no-tests

:: 启动 GUI（Qt 运行库自动加入 PATH）
scripts\run.bat

:: CLI 自检
build\SnapOCR.exe --version
build\SnapOCR.exe --smoke-test
ctest --test-dir build --output-on-failure
```

在 WSL 下调用 Windows 工具链：

```bash
cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat Debug'
```

## 目录结构

```text
SnapOCR/
├── cmake/                  # CMake 辅助模板（Version.h.in）
├── docs/                   # 计划、需求、技术设计、路线图、规范、ADR
├── scripts/                # build.bat / run.bat
├── src/
│   ├── app/                # 应用控制器（命令行、装配）
│   ├── capture/            # 屏幕捕获（M1）
│   ├── overlay/            # 逐屏覆盖层与选区（M1）
│   ├── editor/ annotate/   # 标注编辑器与文档模型（M2）
│   ├── ocr/                # OCR 引擎抽象与实现（M3/P1）
│   ├── ui/                 # 主窗口、设置、结果窗、工具条
│   ├── platform/win32/     # 热键、DPI、剪贴板、自启等 Win32 封装
│   └── common/             # 配置、日志、几何工具
├── tests/                  # Qt Test 单元测试
├── CMakeLists.txt
├── PLAN.md                 # 总计划（单一入口）
└── README.md
```

## 文档

| 文档 | 说明 |
|------|------|
| [PLAN.md](PLAN.md) | 总计划：决策记录、功能范围、里程碑与验收、风险 |
| [docs/README.md](docs/README.md) | 文档索引与权威顺序 |
| [产品需求](docs/01-product-requirements.md) | 场景、需求编号（F-xx/N-xx）、快捷键、验收标准 |
| [技术设计](docs/02-technical-design.md) | 架构、捕获/覆盖层/标注/OCR/系统集成方案 |
| [路线图](docs/03-roadmap.md) | M0–M5 任务分解与出口条件 |
| [开发规范](docs/04-dev-conventions.md) | 提交、代码风格、测试与文档约定 |
| [ADR-0001](docs/adr/0001-tech-stack.md) | 技术栈选型决策记录 |

## 许可

当前为私有开发阶段，暂未添加开源许可证。后续若开源，建议使用 MIT（第三方依赖许可证见技术设计文档）。Qt 采用 LGPLv3/GPLv3 双许可，分发闭源程序需遵守 LGPLv3（动态链接、声明与可替换方式）。
