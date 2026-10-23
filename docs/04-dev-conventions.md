# 开发规范

> 适用于 SnapOCR 仓库全部代码、文档与提交。与 `docs/README.md` 的权威顺序配合使用。

## 1. 语言与编码

- 文档、注释、提交说明使用**简体中文**；代码标识符、文件名、Git 分支使用英文。
- 源码编码 UTF-8，无 BOM；源码中**直接写汉字**，禁止用 `\uXXXX` 转义 CJK。
- MSVC 统一 `/utf-8`；CMake 已为所有目标加该选项，新增目标必须复用 `snapocr_set_common_options`。
- 换行：仓库内 LF；`.bat/.cmd/.ps1` 由 `.gitattributes` 固定为 CRLF。

## 2. 分支与提交

- 主分支 `main` 保持可构建、测试通过；功能/修复用短生命周期分支：
  `feature/m1-capture`、`fix/dpi-offset`、`docs/ocr-benchmark`。
- 提交信息遵循 **Conventional Commits**：

  ```text
  <type>(<scope>): <中文简述>

  <可选正文：为什么、影响范围、验证方式>
  ```

  常用 type：`feat` `fix` `docs` `refactor` `test` `build` `chore` `perf` `style`；
  常用 scope：`capture` `overlay` `editor` `annotate` `ocr` `platform` `app` `ui` `tests` `ci` `docs`。
- 一次提交聚焦一件事；格式化/重命名等纯机械改动单独提交。
- **提交需用户明确要求**；Agent 协作时不得擅自 `git add`/`commit`/`push` 工作区修改（详见 `AGENTS.md`）。
- 禁止提交构建产物、日志、模型临时文件、个人配置（见 `.gitignore`）。

## 3. 构建与验证

提交前必须：

1. `scripts\build.bat`（Debug，含 ctest）通过；或至少 `scripts\build.bat Release --no-tests` + 相关单测。
2. 涉及 UI/截图的改动，附真实 Windows 平台的启动/截图验证说明（WSL 下可用 `cmd.exe /c ...` 调 Windows 工具链）。
3. 新增/修改行为时更新对应文档（需求编号、技术设计、路线图状态）。

WSL 构建示例：

```bash
cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat Debug'
```

## 4. 代码风格

- 以仓库根 `.clang-format` 为准（4 空格、100 列、Allman 大括号）；新文件先格式化再提交。
- 命名：
  - 类/结构体/枚举 `PascalCase`，函数 `camelCase`，成员 `m_camelCase`，常量 `kPascalCase`；
  - 文件名与主要类同名（`MainWindow.h/.cpp`）；
  - 命名空间 `snapocr::<模块>`。
- C++20：优先 `std::optional`/`enum class`/RAII；禁止裸 `new/delete`（Qt 父子对象除外）；禁止异常跨 Qt 信号槽边界。
- Qt：
  - 对象尽量用父对象管理生命周期；非 QObject 用 `std::unique_ptr`；
  - 字符串用 `QStringLiteral` / `QString::fromUtf8`，UI 文本一律 `tr()`；
  - 信号槽连接使用函数指针语法（编译期检查）；
  - 不在主线程做 >16ms 的阻塞操作（捕获、OCR、磁盘大文件）。
- Windows：
  - Win32 API 统一封装在 `src/platform/win32/`，返回错误码/临时错误消息，不抛异常；
  - 坐标以**物理像素**为权威（见技术设计 §4），UI 边界处才换算；
  - 句柄/DC/HWND 使用 RAII 包装，禁止泄漏。
- 注释解释“为什么/约束”，不重复代码字面；复杂算法给出公式或示意。

## 5. 错误处理与日志

- 用户可见错误：中文、可理解、给出下一步（如“热键被占用，请在设置中更换”）；禁止把原始 Win32 错误码直接甩给用户。
- 内部错误：`qCWarning`/`qCCritical` + 日志文件；日志按模块分类（`snapocr.capture`、`snapocr.ocr` 等）。
- 日志中**禁止**记录 OCR 识别原文、剪贴板内容、用户截图路径中的敏感信息（隐私）。
- 所有失败路径必须有单测或手测覆盖，不允许静默失败。

## 6. 测试

- 新核心逻辑（坐标换算、选区模型、撤销重做、结果整理、配置）必须配 Qt Test 单测。
- 渲染类改动补黄金图像测试（`tests/data/golden/`），使用像素哈希/容差比较。
- 修 bug 先补回归测试，再修。
- 手测清单（多屏/DPI/热键/托盘）在 `docs/03-roadmap.md` 对应里程碑中维护，发版前逐项勾选。
- CI 必须绿；本地 `ctest --test-dir build --output-on-failure` 必须通过。

## 7. 文档

- 文档结构与权威顺序见 `docs/README.md`；新文档按分类归档，不直接放 `docs/` 根目录。
- 需求变更：`docs/01-product-requirements.md` + `PLAN.md` 范围表同步。
- 架构/契约变更：先改 `docs/02-technical-design.md`，重大决策补 `docs/adr/NNNN-*.md`。
- 里程碑状态：更新 `docs/03-roadmap.md` 与 `README.md` 状态表。
- 文档中的命令、路径、版本号必须可复现；改动构建方式时同步 README 与技术设计。

## 8. 安全与隐私（硬约束）

- 默认零联网、无遥测、无账号；不得引入后台网络请求（P2 翻译/更新需显式设计并默认关闭）。
- 不申请管理员权限；不安装全局键盘钩子；不注入其他进程。
- 不读取与本工具无关的用户文件；OCR/截图内容仅在本进程内处理。
- 第三方依赖必须登记到技术设计 §14（版本、许可证、用途）。

## 9. 评审检查单（PR/合并前）

- [ ] 构建 + `ctest` 通过（CI 绿）。
- [ ] 需求编号与验收要点已对照（PRD）。
- [ ] 新增坐标计算有单测；UI 改动在多屏/DPI 场景自测。
- [ ] 无阻塞主线程的操作；无内存/GDI 句柄泄漏。
- [ ] 错误路径有中文提示与日志；无敏感信息入日志。
- [ ] 文档（需求/设计/路线图/README）已同步。
- [ ] 无构建产物、无临时文件、无无关格式化噪声。
