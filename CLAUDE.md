# CLAUDE.md

本仓库的协作约定统一维护在 [AGENTS.md](AGENTS.md)。**开始工作前请先阅读该文件。**

核心提醒：

- 项目：Windows 截图 + 标注 + OCR 工具（C++20 / Qt 6.11 / MSVC / CMake + Ninja）。
- 当前阶段与实际进度：`docs/04-完成情况.md`。
- 四份开发文档：`docs/01-需求文档.md`、`docs/02-设计文档.md`、`docs/03-开发文档.md`、`docs/04-完成情况.md`（权威顺序：需求 → 设计 → 开发 → 完成情况）。
- 构建/测试入口：`scripts\build.bat`；WSL 下用 `cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat'`。
- 中文文档与注释；提交需用户明确要求；默认零联网、免管理员权限。
