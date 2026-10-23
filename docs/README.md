# SnapOCR 文档索引

本目录存放项目的计划、需求、技术设计与开发规范。

## 权威顺序

改动代码或行为前，按以下优先级查阅/更新文档：

1. [`../PLAN.md`](../PLAN.md) —— 总计划：定位、决策、范围、里程碑、风险
2. [`01-product-requirements.md`](01-product-requirements.md) —— **产品需求最高权威**（需求编号 F-xx / N-xx 与验收标准）
3. [`02-technical-design.md`](02-technical-design.md) —— 架构与关键技术契约（捕获/覆盖层/标注/OCR/系统集成）
4. [`03-roadmap.md`](03-roadmap.md) —— 任务分解、里程碑出口条件、并行分工
5. [`04-dev-conventions.md`](04-dev-conventions.md) —— 分支、提交、代码风格、测试与文档约定
6. [`adr/`](adr/) —— 架构决策记录（ADR）

## 约定

- 新文档必须归入上述分类，不要直接丢在 `docs/` 根目录；目录/文件名用英文 kebab-case，正文用简体中文。
- 需求变化：先改 `01-product-requirements.md` 与 `PLAN.md` 的范围表，再改代码与测试。
- 架构/契约变化：先加/改 ADR 或 `02-technical-design.md`，再改实现。
- 里程碑任务变化：同步 `03-roadmap.md` 与 `PLAN.md` 的状态表。
- 文档中的代码片段是契约示意，最终以代码为准；契约变更必须回填文档。

## 文件一览

| 文件 | 说明 |
|------|------|
| [`01-product-requirements.md`](01-product-requirements.md) | 用户场景、竞品、需求编号、快捷键、非功能需求、验收 |
| [`02-technical-design.md`](02-technical-design.md) | 架构分层、模块接口、关键流程、DPI/捕获/OCR 方案、打包、测试 |
| [`03-roadmap.md`](03-roadmap.md) | M0–M5 任务清单与出口条件、Agent Teams 并行写域 |
| [`04-dev-conventions.md`](04-dev-conventions.md) | 工程规范与协作约定 |
| [`adr/0001-tech-stack.md`](adr/0001-tech-stack.md) | C++/Qt、GDI 捕获、Windows OCR + RapidOCR 的选型决策 |
