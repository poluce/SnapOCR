# 路线图与任务分解

> 状态：M0 已完成（2026-10-23）。本文件是任务与验收的权威来源；范围变化先改这里与 `PLAN.md`。
> 需求编号引用 [产品需求](01-product-requirements.md)，技术细节引用 [技术设计](02-technical-design.md)。

## 1. 里程碑总览

| 里程碑 | 主题 | 状态 | 出口条件摘要 |
|--------|------|------|--------------|
| M0 | 仓库与骨架 | ✅ 2026-10-23 | 构建/单测/冒烟全绿；文档齐；私有远程可用 |
| M1 | 捕获与选区 | ⏳ 下一阶段 | `F1`→框选→复制/保存闭环；多屏混合 DPI 通过 |
| M2 | 标注编辑 | 规划 | 9 类工具 + 撤销重做 + 1:1 导出 |
| M3 | OCR | 规划 | 取字闭环 + 准确率基准达标 |
| M4 | 产品化 | 规划 | 托盘/设置/热键/单实例/自启/主题/贴图/历史 |
| M5 | 增强与发布 | 规划 | RapidOCR、长截图、性能达标、绿色包/CI 产物 |

依赖关系：M1 → M2 →（M3 可与 M2 并行开发 OCR 核心，但 UI 集成依赖 M2 工具条）→ M4 → M5。

## 2. M0 仓库与骨架（✅ 已完成）

- [x] 初始化仓库 `D:\Document\SnapOCR`（`main` 分支，Conventional Commits）。
- [x] 工程配置：`.gitignore`、`.gitattributes`（bat=CRLF）、`.editorconfig`、`.clang-format`。
- [x] CMake 骨架：`snapocr_core` 静态库 + `SnapOCR.exe`（WIN32 GUI）+ `snapocr_tests`。
- [x] 版本注入：`cmake/Version.h.in` → 生成 `snapocr/Version.h`（0.1.0）。
- [x] 应用控制器：命令行 `--version/--help/--smoke-test`、占位主窗口、控制台挂接。
- [x] Qt Test 单测（版本/单实例键约束）。
- [x] `scripts/build.bat`（vcvars64 + Qt/CMake/Ninja 探测）与 `scripts/run.bat`。
- [x] GitHub Actions `build.yml`（CI 固定 Qt 6.10.3 + MSVC + Ninja + ctest + 原生冒烟；本机开发 Qt 6.11.1）。
- [x] 计划文档：`PLAN.md`、`docs/README.md`、PRD、技术设计、路线图、开发规范、ADR-0001。
- [x] GitHub 私有仓库 `poluce/SnapOCR` 创建并推送。

**M0 验证记录**：

```text
scripts\build.bat Debug
  → CMake Configure done / 14 targets built / ctest 2/2 passed
  → SnapOCR.exe --smoke-test: platform=windows, PASS (exit 0)
  → SnapOCR.exe --version: SnapOCR 0.1.0
```

## 3. M1 捕获与选区（下一阶段）

**目标**：`F1` → 冻结画面 → 框选 → `Enter` 复制 PNG / `Ctrl+S` 保存，多屏与混合 DPI 正确。

### 3.1 契约与平台（先定接口，后并行）

- [ ] 新增 `src/capture/CaptureTypes.h`：`MonitorInfo`、`ScreenFrame`、`CaptureResult`（物理像素契约，见技术设计 §5.3）。
- [ ] `MonitorLayout`：`EnumDisplayMonitors` 枚举物理矩形、主屏、DPR、旋转；虚拟桌面并集；单元测试（mock 布局数据）。
- [ ] `Win32ScreenCapture`：逐屏 `BitBlt` + DIB → `QImage`；GDI/DC RAII；失败错误码。
- [ ] `HotkeyManager`：隐藏消息窗 + `RegisterHotKey` + `QAbstractNativeEventFilter`；`F1` 注册；冲突返回错误。
- [ ] `CaptureService`：异步捕获、取消、结果信号；热键回调链路打通。

### 3.2 覆盖层与选区

- [ ] `OverlayController`：按 `CaptureResult.screens` 创建每屏覆盖窗口；生命周期与图片释放。
- [ ] `OverlayWindow`：显示冻结画面（1:1 设备像素）、暗化遮罩、选区边框、8 控制点、尺寸/坐标 HUD。
- [ ] `SelectionModel`：物理/逻辑坐标双记录；拖动、缩放、方向键（1px / Shift 10px）；边界约束。
- [ ] 跨屏框选：鼠标 grab + 全局坐标换算；跨屏时各覆盖窗口正确绘制选区片段。
- [ ] 键盘：`Esc`/右键取消（无残留）、`Enter`/双击确认、`Ctrl+A` 全屏选区（可选）。
- [ ] 工具条占位（M1 仅“复制/保存/取消”，标注按钮置灰）。
- [ ] 自身窗口排除：捕获前隐藏/标记 SnapOCR 窗口；自我截图不出现。

### 3.3 输出

- [ ] 选区裁剪（物理像素）→ `QImage`；`QMimeData` 写入 PNG + DIB，复制成功提示。
- [ ] `Ctrl+S` 快速保存：默认目录/格式/文件名模板（基础版，完整设置项 M4）。
- [ ] PNG/JPG 编码与失败提示（磁盘/权限）。
- [ ] 截图后释放 `CaptureResult` 大图内存，覆盖层窗口全部销毁。

### 3.4 测试与验收

- [ ] 单测：`MonitorLayout` 坐标并集/换算、`SelectionModel` 拖动缩放边界、DPR 映射（100/125/150/200 模拟）。
- [ ] 手测矩阵：单屏 100/125/150/200%；双屏同 DPI；双屏混合 DPI；副屏在左/上（负坐标）。
- [ ] `F1`→框选→`Ctrl+C` 粘贴到 QQ/微信/Word 图片正常；`Ctrl+S` 保存文件可打开。
- [ ] `Esc` 取消后无窗口残留、托盘正常、可再次截图。
- [ ] 性能：4K 单屏热键→遮罩 ≤ 80ms（目标）/150ms（可接受）；空闲内存记录基线。

**M1 出口**：上述验收全部通过；`ctest` 与 CI 绿；技术设计中的开放问题 1–3 定稿并回填文档。

## 4. M2 标注编辑

- [ ] `AnnotationDocument` / `AnnotationItem` / `AnnotationStyle`（技术设计 §7.1）；`hitTest`。
- [ ] `AnnotationCanvas`：原图 1:1 渲染、DPR 正确、缩放窗口自适应。
- [ ] 工具：矩形、椭圆、直线、箭头、画笔、文字、序号、高亮、马赛克（F-ANN-01–09）。
- [ ] 选择工具：命中/选中高亮/拖动移动/`Delete` 删除（F-ANN-10）。
- [ ] 撤销/重做：快照式 `QUndoStack`，≥50 步（F-ANN-11）。
- [ ] 样式：颜色预设 + 自定义、线宽、字号；作用于新图元与选中图元（F-ANN-12）。
- [ ] 文字输入：画布内嵌 `QTextEdit`，支持中文/换行/取消。
- [ ] 工具条：工具切换、撤销/重做、颜色/线宽、复制/保存/取字/取消；快捷键 `1`–`8`。
- [ ] 导出：`AnnotationDocument::render()` → PNG/JPG；`Enter`/`Ctrl+C` 复制；`Ctrl+S` 保存；另存为。
- [ ] 单测：图元几何、命中测试、撤销重做、渲染尺寸 1:1；黄金图像（矩形/箭头/文字/马赛克）。
- [ ] 手测：9 类工具逐一验收；导出图像与屏幕区域像素一致（局部比对）。

**M2 出口**：F-ANN P0 全部可用；黄金图像测试稳定；复制/保存/取字入口贯通（OCR 接 M3）。

## 5. M3 OCR

- [ ] `IOcrEngine` / `OcrLanguage` / `OcrLine` / `OcrResult`（技术设计 §8.1）。
- [ ] `WindowsOcrEngine`：C++/WinRT 语言枚举、`TryCreateFromLanguage`、QImage→BGRA8→`SoftwareBitmap`、worker 线程。
- [ ] `OcrService`：异步调度、超时（默认 10s）、结果回主线程、错误映射。
- [ ] 大图切片：超过 `MaxImageDimension` 时切块 + 重叠合并。
- [ ] OCR 结果窗：按行展示、全选/选择/复制、语言与耗时、重新识别、关闭（F-OCR-03）。
- [ ] 截图工具条“提取文字”入口（F-OCR-01）。
- [ ] `F4` 屏取字流程：框选→识别→默认复制+提示（F-OCR-02/05）。
- [ ] 语言包检测与安装引导（F-OCR-06）；语言选择（系统可用语言）。
- [ ] 基准集 `tests/data/ocr/`：界面文本/正文/字幕各若干；脚本统计字符召回（目标 ≥85%）。
- [ ] 性能：4K 的 1/4 区域端到端 ≤1s；识别中 UI 不卡顿。
- [ ] 单测：OCR 结果行合并、切片重叠去重、语言选择、错误映射（不依赖 WinRT，可 mock）。

**M3 出口**：截图取字与 F4 取字可用；基准达标；未达标则启动 RapidOCR 提前开发（M3.5）。

## 6. M4 产品化

- [ ] 托盘：`QSystemTrayIcon` + 菜单（截图/取字/设置/历史/关于/退出）+ 主题化图标（F-SYS-02）。
- [ ] 单实例：`QLocalServer/QLocalSocket`，二次启动转发命令（F-SYS-03）。
- [ ] 设置对话框：热键、捕获、保存、OCR、主题、高级；`QSettings` 持久化与损坏回退（F-SYS-04）。
- [ ] 热键可配置 + 冲突检测/提示（F-SYS-01）；默认 `F1`/`F4`。
- [ ] 开机自启：HKCU Run 写入/删除 + 设置同步（F-SYS-05）。
- [ ] 主题：浅/深/跟随系统，覆盖层与结果窗统一（F-SYS-06）。
- [ ] 保存设置完整化：目录选择、格式、文件名模板、JPG 质量（F-SYS-07）。
- [ ] 日志与错误提示：文件轮转、级别、崩溃捕获（F-SYS-08）。
- [ ] 贴图（P1）：置顶窗、拖动/缩放/透明度/旋转、`Esc` 关闭、可再标注/取字（F-ANN-19）。
- [ ] 历史（P1）：最近 50 张缩略图、重编辑、复制、删除、清理（F-CAP-17）。
- [ ] 托盘行为：关闭主窗隐藏、退出一致性；首次运行引导。
- [ ] 手测：自启、单实例、热键冲突、主题切换、贴图/历史、崩溃提示。

**M4 出口**：F-SYS P0 全部通过；P1 贴图/历史可用；设置重启保留；安装绿色包在干净环境可运行。

## 7. M5 增强与发布

- [ ] `RapidOcrEngine`（ONNX Runtime + det/cls/rec 模型）+ 设置切换（F-OCR-10）。
- [ ] OCR 基准对比（Windows vs RapidOCR）并记录结论；确定默认引擎。
- [ ] 长截图（P1，F-CAP-16）——若时间不足可延后，但需更新路线图状态。
- [ ] 性能专项：热键→遮罩、OCR、内存达到预算；必要时引入 DXGI 捕获（记录 ADR）。
- [ ] `windeployqt` 打包脚本 + 绿色 zip；CI 产物 artifact（F-SYS P2 安装包可延后）。
- [ ] 完整手测矩阵 + 干净 Win10/11 环境验证；发布说明。
- [ ] 文档收尾：README 截图/演示、路线图状态、已知问题。

**M5 出口**：性能目标达标；绿色包可在干净环境运行；CI 可下载产物；文档同步。

## 8. 并行分工（M1 起，Agent Teams）

共享契约先由 Lead 定稿（`CaptureResult`、`SelectionModel`、`AnnotationDocument`、`IOcrEngine`、`Settings`），再分线并行：

| 线 | 写域（workspace 相对） | 职责 | 依赖 |
|----|------------------------|------|------|
| A 捕获/覆盖层 | `src/capture/`、`src/overlay/`、`src/platform/win32/` | 显示器枚举、GDI 捕获、逐屏覆盖层、选区交互、DPI 换算 | 契约 |
| B 标注/编辑器 | `src/annotate/`、`src/editor/` | 图元模型、画布渲染、工具、撤销重做、导出 | 契约 + A 的选区结果 |
| C OCR | `src/ocr/` | 引擎抽象、Windows OCR、切片、结果整理、基准工具 | 契约（可与 A/B 并行） |
| D 系统/测试/CI | `src/app/`、`src/ui/`、`tests/`、`scripts/`、`.github/` | 热键/托盘/设置/单实例/自启、单测与手测清单、CI | A/B/C 接口 |

协作规则：写域不重叠；跨线接口改动先提 ADR/技术设计变更；集成分支由 Lead 统一构建与评审；每日合并前跑 `scripts\build.bat` + `ctest`。

## 9. 工作量估算（人日，AI 协作会显著压缩）

| 阶段 | 估算 | 说明 |
|------|------|------|
| M0 | 0.5 | 已完成 |
| M1 | 2.5 | 捕获/覆盖层/DPI 是难点 |
| M2 | 3.5 | 工具与撤销/命中测试工作量大 |
| M3 | 2.0 | Windows OCR 集成 + 基准 |
| M4 | 2.5 | 托盘/设置/贴图/历史 |
| M5 | 2.5 | RapidOCR/打包/性能 |
| 合计 | ≈13.5 | 不含长截图等 P1 可选项 |

## 10. 变更记录

| 日期 | 变更 |
|------|------|
| 2026-10-23 | 创建路线图；M0 完成；确定 M1 任务与验收 |
