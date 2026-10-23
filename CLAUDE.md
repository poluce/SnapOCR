# CLAUDE.md

本仓库的协作约定统一维护在 [AGENTS.md](AGENTS.md)。**开始工作前请先阅读该文件。**

核心提醒：

- 项目：Windows 截图 + 标注 + OCR 工具（C++20 / Qt 6.11 / MSVC / CMake + Ninja）。
- 当前阶段与任务：`PLAN.md`、`docs/03-roadmap.md`。
- 构建/测试入口：`scripts\build.bat`；WSL 下用 `cmd.exe /c 'cd /d D:\Document\SnapOCR && scripts\build.bat'`。
- 文档权威顺序：`PLAN.md` → `docs/01` 需求 → `docs/02` 技术契约 → `docs/03` 路线图 → `docs/04` 规范 → 代码。
- 中文文档与注释；提交需用户明确要求；默认零联网、免管理员权限。
